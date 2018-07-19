// bthread - A M:N threading library to make applications more concurrent.
// Copyright (c) 2014 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Ge,Jun (gejun@baidu.com)
// Date: Sun Jul 13 15:04:18 CST 2014

#ifndef BUTIL_RESOURCE_POOL_INL_H
#define BUTIL_RESOURCE_POOL_INL_H

#include <iostream>                      // std::ostream
#include <pthread.h>                     // pthread_mutex_t
#include <algorithm>                     // std::max, std::min
#include "butil/atomicops.h"              // butil::atomic
#include "butil/macros.h"                 // BAIDU_CACHELINE_ALIGNMENT
#include "butil/scoped_lock.h"            // BAIDU_SCOPED_LOCK
#include "butil/thread_local.h"           // thread_atexit
#include <vector>

// "资源内存池"中的"全局空闲资源标识符"分配数自增 +1
#ifdef BUTIL_RESOURCE_POOL_NEED_FREE_ITEM_NUM
#define BAIDU_RESOURCE_POOL_FREE_ITEM_NUM_ADD1                \
    (_global_nfree.fetch_add(1, butil::memory_order_relaxed))
#define BAIDU_RESOURCE_POOL_FREE_ITEM_NUM_SUB1                \
    (_global_nfree.fetch_sub(1, butil::memory_order_relaxed))
#else
#define BAIDU_RESOURCE_POOL_FREE_ITEM_NUM_ADD1
#define BAIDU_RESOURCE_POOL_FREE_ITEM_NUM_SUB1
#endif

namespace butil {

// 资源标识符（与对象一一对应）
template <typename T>
struct ResourceId {
    uint64_t value; // 64-bit 唯一标识符

    operator uint64_t() const {
        return value;
    }

    // 任意类型转换
    template <typename T2>
    ResourceId<T2> cast() const {
        ResourceId<T2> id = { value };
        return id;
    }
};

// 固定大小与"动态"大小的"空闲资源标识符"列表。
template <typename T, size_t NITEM> 
struct ResourcePoolFreeChunk {
    size_t nfree;
    ResourceId<T> ids[NITEM];
};
// for gcc 3.4.5
template <typename T> 
struct ResourcePoolFreeChunk<T, 0> {
    size_t nfree;
    ResourceId<T> ids[0];   // 柔性数组
};

// "资源内存池"信息统计
struct ResourcePoolInfo {
    size_t local_pool_num;
    size_t block_group_num;
    size_t block_num;
    size_t item_num;
    size_t block_item_num;
    size_t free_chunk_item_num;
    size_t total_size;
#ifdef BUTIL_RESOURCE_POOL_NEED_FREE_ITEM_NUM
    size_t free_item_num;
#endif
};


// "资源内存池"能容纳"内存块分组"最大个数
static const size_t RP_MAX_BLOCK_NGROUP = 65536;

// 每个"内存块分组"能容纳最大"内存块 Block"个数
static const size_t RP_GROUP_NBLOCK_NBIT = 16;
static const size_t RP_GROUP_NBLOCK = (1UL << RP_GROUP_NBLOCK_NBIT);

// 初始化动态"空闲资源标识符"个数的内存容量
static const size_t RP_INITIAL_FREE_LIST_SIZE = 1024;

// 每个"内存块 Block"实际能容纳最大对象的个数。即：
// 取 ResourcePoolBlockMaxSize/sizeof(T), ResourcePoolBlockMaxItem 较小者。
template <typename T>
class ResourcePoolBlockItemNum {
    static const size_t N1 = ResourcePoolBlockMaxSize<T>::value / sizeof(T);
    static const size_t N2 = (N1 < 1 ? 1 : N1);
public:
    static const size_t value = (N2 > ResourcePoolBlockMaxItem<T>::value ?
                                 ResourcePoolBlockMaxItem<T>::value : N2);
};

// "资源内存池"
template <typename T>
class BAIDU_CACHELINE_ALIGNMENT ResourcePool {
public:
    // 每个"内存块 Block"实际能容纳最大对象的个数。
    static const size_t BLOCK_NITEM = ResourcePoolBlockItemNum<T>::value;
    // "空闲资源标识符"个数（用来索引"内存块 Block"中对象，故数量与其相同）。
    static const size_t FREE_CHUNK_NITEM = BLOCK_NITEM;

    // Free identifiers are batched in a FreeChunk before they're added to
    // global list(_free_chunks).
    // 
    // "空闲资源标识符"类型
    typedef ResourcePoolFreeChunk<T, FREE_CHUNK_NITEM>      FreeChunk;
    typedef ResourcePoolFreeChunk<T, 0> DynamicFreeChunk;

    // When a thread needs memory, it allocates a Block. To improve locality,
    // items in the Block are only used by the thread.
    // To support cache-aligned objects, align Block.items by cacheline.
    // 
    // 当一个线程需要内存时，以"内存块 Block"向操作系统申请内存。为了改善局部性，块中的 
    // items 仅供分配线程使用（_local_pool 为线程级单例）。
    struct BAIDU_CACHELINE_ALIGNMENT Block {
        // 实际内存池。最多可容纳 BLOCK_NITEM 个 T 类型对象。
        char items[sizeof(T) * BLOCK_NITEM];

        // 内存池中实际已分配对象个数
        size_t nitem;

        Block() : nitem(0) {}
    };

    // A Resource addresses at most RP_MAX_BLOCK_NGROUP BlockGroups,
    // each BlockGroup addresses at most RP_GROUP_NBLOCK blocks. So a
    // resource addresses at most RP_MAX_BLOCK_NGROUP * RP_GROUP_NBLOCK Blocks.
    // 
    // 一个"资源内存池"上至多有 RP_MAX_BLOCK_NGROUP 的 BlockGroup 。每个 BlockGroup 
    // 上至多有 RP_GROUP_NBLOCK 的 "内存块 Block"。
    // 
    // 所以一个"资源内存池"最多可以处理 RP_MAX_BLOCK_NGROUP * RP_GROUP_NBLOCK 的"内
    // 存块 Block"。
    struct BlockGroup {
        // 已分配"内存块 Block"个数。
        butil::atomic<size_t> nblock;
        // "内存块 Block"指针数组。
        butil::atomic<Block*> blocks[RP_GROUP_NBLOCK];

        BlockGroup() : nblock(0) {
            // We fetch_add nblock in add_block() before setting the entry,
            // thus address_resource() may sees the unset entry. Initialize
            // all entries to NULL makes such address_resource() return NULL.
            // 
            // 初始化所有"内存块 Block"的指针为 NULL ，以使得 address_resource() 返
            // 回 NULL 。
            memset(blocks, 0, sizeof(butil::atomic<Block*>) * RP_GROUP_NBLOCK);
        }
    };


    // Each thread has an instance of this class.
    // 
    // 用于实际的执行"资源内存池"中的对象内存分配任务（系统申请的内存还是全局的）。每个线
    // 程都有这该实例，从而也实现了线程隔离。
    class BAIDU_CACHELINE_ALIGNMENT LocalPool {
    public:
        explicit LocalPool(ResourcePool* pool)
            : _pool(pool)
            , _cur_block(NULL)
            , _cur_block_index(0) {
            _cur_free.nfree = 0;
        }

        ~LocalPool() {
            // Add to global _free_chunks if there're some free resources
            // 
            // 将当前"空闲资源标识"节点 |_cur_free| 压入动态"空闲资源标识"节点
            // 内存中 |_free_chunks|
            if (_cur_free.nfree) {
                _pool->push_free_chunk(_cur_free);
            }

            _pool->clear_from_destructor_of_local_pool();
        }

        // 线程退出函数。清理线程本地内存池对象。
        static void delete_local_pool(void* arg) {
            delete(LocalPool*)arg;
        }

        // We need following macro to construct T with different CTOR_ARGS
        // which may include parenthesis because when T is POD, "new T()"
        // and "new T" are different: former one sets all fields to 0 which
        // we don't want.
        // 
        // 获取/新建 Block 对象。使用不同的 CTOR_ARGS 参数 T 的构造函数来构造对象。
        // 其中 CTOR_ARGS 包括圆括号。因为当 T 是 POD 时，"new T()" 和 "new T" 
        // 是不同的：前者将所有字段设置为 0 。
#define BAIDU_RESOURCE_POOL_GET(CTOR_ARGS)                              \
        /* Fetch local free id 获取当前空闲"资源标识符"关联的资源对象*/       \
        if (_cur_free.nfree) {                                          \
            const ResourceId<T> free_id = _cur_free.ids[--_cur_free.nfree]; \
            *id = free_id;                                              \
            BAIDU_RESOURCE_POOL_FREE_ITEM_NUM_SUB1;                   \
            return unsafe_address_resource(free_id);                    \
        }                                                               \
        /* Fetch a FreeChunk from global.                               \
           TODO: Popping from _free needs to copy a FreeChunk which is  \
           costly, but hardly impacts amortized performance. 从动态"资源标识符"获取关联的资源对象*/ \
        if (_pool->pop_free_chunk(_cur_free)) {                         \
            --_cur_free.nfree;                                          \
            const ResourceId<T> free_id =  _cur_free.ids[_cur_free.nfree]; \
            *id = free_id;                                              \
            BAIDU_RESOURCE_POOL_FREE_ITEM_NUM_SUB1;                   \
            return unsafe_address_resource(free_id);                    \
        }                                                               \
        /* Fetch memory from local block 从当前"内存块"分配一个 T 对象地址*/ \
        if (_cur_block && _cur_block->nitem < BLOCK_NITEM) {            \
            id->value = _cur_block_index * BLOCK_NITEM + _cur_block->nitem; \
            T* p = new ((T*)_cur_block->items + _cur_block->nitem) T CTOR_ARGS; \
            if (!ResourcePoolValidator<T>::validate(p)) {               \
                p->~T();                                                \
                return NULL;                                            \
            }                                                           \
            ++_cur_block->nitem;                                        \
            return p;                                                   \
        }                                                               \
        /* Fetch a Block from global 创建获取一个"内存块"，分配 T 对象地址*/ \
        _cur_block = add_block(&_cur_block_index);                      \
        if (_cur_block != NULL) {                                       \
            id->value = _cur_block_index * BLOCK_NITEM + _cur_block->nitem; \
            T* p = new ((T*)_cur_block->items + _cur_block->nitem) T CTOR_ARGS; \
            if (!ResourcePoolValidator<T>::validate(p)) {               \
                p->~T();                                                \
                return NULL;                                            \
            }                                                           \
            ++_cur_block->nitem;                                        \
            return p;                                                   \
        }                                                               \
        return NULL;                                                    \
 
        
        /** get(ResourceId<T>* id, ...) 获取一 T 类型对象内存资源地址，标识符写入 |id| */

        inline T* get(ResourceId<T>* id) {
            BAIDU_RESOURCE_POOL_GET();
        }

        template <typename A1>
        inline T* get(ResourceId<T>* id, const A1& a1) {
            BAIDU_RESOURCE_POOL_GET((a1));
        }

        template <typename A1, typename A2>
        inline T* get(ResourceId<T>* id, const A1& a1, const A2& a2) {
            BAIDU_RESOURCE_POOL_GET((a1, a2));
        }

#undef BAIDU_RESOURCE_POOL_GET

        // 保存已关联资源的"资源标识符 |id|"(get_resource() 返回)到当前空闲"资源标识符"节
        // 点数组中供以后获取。
        inline int return_resource(ResourceId<T> id) {
            // Return to local free list
            // 
            // 当前"空闲资源标识符"链表。
            if (_cur_free.nfree < ResourcePool::free_chunk_nitem()) {
                // 写入"资源标识符 |id|"到当前空闲"资源标识符"链表
                _cur_free.ids[_cur_free.nfree++] = id;
                BAIDU_RESOURCE_POOL_FREE_ITEM_NUM_ADD1;
                return 0;
            }
            // Local free list is full, return it to global.
            // For copying issue, check comment in upper get()
            // 
            // 将当前空闲"资源标识符"节点 |_cur_free| 拷贝到全局 _pool 动态"空闲节点" 
            // _pool::_free_chunks 内存中。
            if (_pool->push_free_chunk(_cur_free)) {
                // 重置 _cur_free 链表
                _cur_free.nfree = 1;
                _cur_free.ids[0] = id;
                BAIDU_RESOURCE_POOL_FREE_ITEM_NUM_ADD1;
                return 0;
            }
            return -1;
        }

    private:
        // "资源内存池"地址（进程级单例）
        ResourcePool* _pool;

        // 当前正在分配的"内存块 Block"地址，指向 _pool::_block_groups::_blocks 某个块
        Block* _cur_block;

        // _pool::_block_groups::_blocks 某个块的索引（假设被当作一维数组。相对于 
        // _pool::_block_groups 首地址）。
        size_t _cur_block_index;

        // 当前空闲"资源标识符"节点数组。数量与单个"内存块 Block"能容纳 T 类型对象个数相等。
        FreeChunk _cur_free;
    };

    // 根据 |id| 获取关联的"内存块 Block"中具体内存地址
    static inline T* unsafe_address_resource(ResourceId<T> id) {
        const size_t block_index = id.value / BLOCK_NITEM;
        return (T*)(_block_groups[(block_index >> RP_GROUP_NBLOCK_NBIT)]
                    .load(butil::memory_order_consume)
                    ->blocks[(block_index & (RP_GROUP_NBLOCK - 1))]
                    .load(butil::memory_order_consume)->items) +
               id.value - block_index * BLOCK_NITEM;
    }

    static inline T* address_resource(ResourceId<T> id) {
        const size_t block_index = id.value / BLOCK_NITEM;
        const size_t group_index = (block_index >> RP_GROUP_NBLOCK_NBIT);
        if (__builtin_expect(group_index < RP_MAX_BLOCK_NGROUP, 1)) {
            BlockGroup* bg =
                _block_groups[group_index].load(butil::memory_order_consume);
            if (__builtin_expect(bg != NULL, 1)) {
                Block* b = bg->blocks[block_index & (RP_GROUP_NBLOCK - 1)]
                           .load(butil::memory_order_consume);
                if (__builtin_expect(b != NULL, 1)) {
                    const size_t offset = id.value - block_index * BLOCK_NITEM;
                    if (__builtin_expect(offset < b->nitem, 1)) {
                        return (T*)b->items + offset;
                    }
                }
            }
        }

        return NULL;
    }

    /** get_resource(ResourceId<T>* id, ...) 获取一 T 类型对象内存资源地址，标识符写入 |id|  */

    inline T* get_resource(ResourceId<T>* id) {
        // 获取或者新建本地域线程对象 (LocalPool)
        LocalPool* lp = get_or_new_local_pool();
        if (__builtin_expect(lp != NULL, 1)) {
            // 获取 <T> 类型对象内存资源地址
            return lp->get(id);
        }
        return NULL;
    }

    template <typename A1>
    inline T* get_resource(ResourceId<T>* id, const A1& arg1) {
        LocalPool* lp = get_or_new_local_pool();
        if (__builtin_expect(lp != NULL, 1)) {
            return lp->get(id, arg1);
        }
        return NULL;
    }

    template <typename A1, typename A2>
    inline T* get_resource(ResourceId<T>* id, const A1& arg1, const A2& arg2) {
        LocalPool* lp = get_or_new_local_pool();
        if (__builtin_expect(lp != NULL, 1)) {
            return lp->get(id, arg1, arg2);
        }
        return NULL;
    }

    inline int return_resource(ResourceId<T> id) {
        LocalPool* lp = get_or_new_local_pool();
        if (__builtin_expect(lp != NULL, 1)) {
            return lp->return_resource(id);
        }
        return -1;
    }

    // 删除内存资源池
    void clear_resources() {
        LocalPool* lp = _local_pool;
        if (lp) {
            // "删除"。真正的删除内存动作延迟到线程取消时进行（注册线程取消回调）
            _local_pool = NULL;
            butil::thread_atexit_cancel(LocalPool::delete_local_pool, lp);
            delete lp;
        }
    }

    // 空闲"资源标识符"最大个数。
    static inline size_t free_chunk_nitem() {
        const size_t n = ResourcePoolFreeChunkMaxItem<T>::value();
        return n < FREE_CHUNK_NITEM ? n : FREE_CHUNK_NITEM;
    }
    
    // Number of all allocated objects, including being used and free.
    // 
    // 所有已分配对象的数量，包括正在使用和空闲"资源标识符"
    ResourcePoolInfo describe_resources() const {
        ResourcePoolInfo info;
        info.local_pool_num = _nlocal.load(butil::memory_order_relaxed);
        info.block_group_num = _ngroup.load(butil::memory_order_acquire);
        info.block_num = 0;
        info.item_num = 0;
        info.free_chunk_item_num = free_chunk_nitem();
        info.block_item_num = BLOCK_NITEM;
#ifdef BUTIL_RESOURCE_POOL_NEED_FREE_ITEM_NUM
        info.free_item_num = _global_nfree.load(butil::memory_order_relaxed);
#endif

        for (size_t i = 0; i < info.block_group_num; ++i) {
            BlockGroup* bg = _block_groups[i].load(butil::memory_order_consume);
            if (NULL == bg) {
                break;
            }
            size_t nblock = std::min(bg->nblock.load(butil::memory_order_relaxed),
                                     RP_GROUP_NBLOCK);
            info.block_num += nblock;
            for (size_t j = 0; j < nblock; ++j) {
                Block* b = bg->blocks[j].load(butil::memory_order_consume);
                if (NULL != b) {
                    info.item_num += b->nitem;
                }
            }
        }
        info.total_size = info.block_num * info.block_item_num * sizeof(T);
        return info;
    }

    // 获取"资源内存池"单例（进程级单例）
    static inline ResourcePool* singleton() {
        ResourcePool* p = _singleton.load(butil::memory_order_consume);
        if (p) {
            return p;
        }
        // 创建单例
        pthread_mutex_lock(&_singleton_mutex);
        p = _singleton.load(butil::memory_order_consume);
        if (!p) {
            p = new ResourcePool();
            _singleton.store(p, butil::memory_order_release);
        } 
        pthread_mutex_unlock(&_singleton_mutex);
        return p;
    }

private:
    // 初始化"资源内存池"
    ResourcePool() {
        // 初始化"空闲资源标识符"节点内存容量为 RP_INITIAL_FREE_LIST_SIZE
        _free_chunks.reserve(RP_INITIAL_FREE_LIST_SIZE);
        pthread_mutex_init(&_free_chunks_mutex, NULL);
    }

    ~ResourcePool() {
        pthread_mutex_destroy(&_free_chunks_mutex);
    }

    // Create a Block and append it to right-most BlockGroup.
    // 
    // 创建并返回一个"内存块 Block"，并将其附加到 _block_groups（BlockGroup::blocks） 
    // 的数组尾部。
    static Block* add_block(size_t* index) {
        // 创建一个"内存块 Block"
        Block* const new_block = new(std::nothrow) Block;
        if (NULL == new_block) {
            return NULL;
        }

        // 1. 创建 BlockGroup 并添加到 _block_groups 队列中
        // 2. _block_groups 队列中最多 RP_MAX_BLOCK_NGROUP 个 BlockGroup
        // 3. 将新创建的 Block 添加到该组的 BlockGroup::blocks 中
        // 4. 每个 BlockGroup 中最多 RP_GROUP_NBLOCK 个 Block 
        size_t ngroup;
        do {
            ngroup = _ngroup.load(butil::memory_order_acquire);
            if (ngroup >= 1) {
                // 获取当前 BlockGroup 地址
                BlockGroup* const g =
                    _block_groups[ngroup - 1].load(butil::memory_order_consume);

                // 获取将要分配的"内存块 Block"在当前 BlockGroup 的索引。并自增 +1
                const size_t block_index =
                    g->nblock.fetch_add(1, butil::memory_order_relaxed);

                // 内存块分组最大可分配"内存块 Block"个数不能超过 2^16
                if (block_index < RP_GROUP_NBLOCK) {
                    // 保存系统新申请的"内存块 Block"地址。
                    g->blocks[block_index].store(
                        new_block, butil::memory_order_release);

                    // 该"内存块 Block"相对 _block_groups 首地址索引。即，若把 
                    // _block_groups 里嵌套的 BlockGroup::blocks 看成二维数组，
                    // 则 *index 为 _block_groups::blocks 一维数组的索引。
                    *index = (ngroup - 1) * RP_GROUP_NBLOCK + block_index;
                    return new_block;
                }
                // 还原预递增的"块分配"数量（分配失败，可能当前的"内存块分组"分配块达到最
                // 大值）。
                g->nblock.fetch_sub(1, butil::memory_order_relaxed);
            }
        } while (add_block_group(ngroup));

        // Fail to add_block_group.
        // 
        // 添加到 _block_groups（BlockGroup） 失败
        delete new_block;
        return NULL;
    }

    // Create a BlockGroup and append it to _block_groups.
    // Shall be called infrequently because a BlockGroup is pretty big.
    // 
    // 创建一个 BlockGroup 并将其附加到 _block_groups 数组中。由于 BlockGroup 中
    // 能容纳"内存块 Block"相当大，所以不会被频繁地调用。
    static bool add_block_group(size_t old_ngroup) {
        BlockGroup* bg = NULL;
        BAIDU_SCOPED_LOCK(_block_group_mutex);
        const size_t ngroup = _ngroup.load(butil::memory_order_acquire);
        if (ngroup != old_ngroup) {
            // Other thread got lock and added group before this thread.
            // 
            // 其他线程在已经锁定并添加到 _block_groups 数组中。
            return true;
        }
        // 不能超过 65536 个内存块分组
        if (ngroup < RP_MAX_BLOCK_NGROUP) {
            // 添加一个新的内存块分组
            bg = new(std::nothrow) BlockGroup;
            if (NULL != bg) {
                // Release fence is paired with consume fence in address() and
                // add_block() to avoid un-constructed bg to be seen by other
                // threads.
                // 
                // release fence 与 address()/add_block() 中的 consume fence 配对，
                // 以避免其他线程看到未构造的 bg 。
                _block_groups[ngroup].store(bg, butil::memory_order_release);
                _ngroup.store(ngroup + 1, butil::memory_order_release);
            }
        }
        return bg != NULL;
    }

    // 创建/获取"本地资源内存池"对象 (线程级单例）
    inline LocalPool* get_or_new_local_pool() {
        // 每个"资源内存池"对象，在同一个线程下，只有一个 LocalPool 对象
        LocalPool* lp = _local_pool;
        if (lp != NULL) {
            return lp;
        }
        // 新建 LocalPool 对象
        lp = new(std::nothrow) LocalPool(this);
        if (NULL == lp) {
            return NULL;
        }
        BAIDU_SCOPED_LOCK(_change_thread_mutex); //avoid race with clear()
        _local_pool = lp;
        // 注册线程退出函数（清理"本地资源内存池"对象）
        butil::thread_atexit(LocalPool::delete_local_pool, lp);
        // 记录 LocalPool 对象个数
        _nlocal.fetch_add(1, butil::memory_order_relaxed);
        return lp;
    }

    // 线程退出时清理函数
    void clear_from_destructor_of_local_pool() {
        // Remove tls
        _local_pool = NULL;

        // 是否有其他线程在引用"资源内存池 ResourcePool"
        if (_nlocal.fetch_sub(1, butil::memory_order_relaxed) != 1) {
            return;
        }

        // Can't delete global even if all threads(called ResourcePool
        // functions) quit because the memory may still be referenced by 
        // other threads. But we need to validate that all memory can
        // be deallocated correctly in tests, so wrap the function with 
        // a macro which is only defined in unittests.
#ifdef BAIDU_CLEAR_RESOURCE_POOL_AFTER_ALL_THREADS_QUIT
        BAIDU_SCOPED_LOCK(_change_thread_mutex);  // including acquire fence.
        // Do nothing if there're active threads.
        if (_nlocal.load(butil::memory_order_relaxed) != 0) {
            return;
        }
        // All threads exited and we're holding _change_thread_mutex to avoid
        // racing with new threads calling get_resource().

        // Clear global free list.
        // 
        // 清除所有动态"空闲资源标识符"链表
        FreeChunk dummy;
        while (pop_free_chunk(dummy));

        // Delete all memory
        // 
        // 调用所有对象析构，删除所有"内存块分组"内存、"内存块 Block" 内存
        const size_t ngroup = _ngroup.exchange(0, butil::memory_order_relaxed);
        for (size_t i = 0; i < ngroup; ++i) {
            BlockGroup* bg = _block_groups[i].load(butil::memory_order_relaxed);
            if (NULL == bg) {
                break;
            }
            size_t nblock = std::min(bg->nblock.load(butil::memory_order_relaxed),
                                     RP_GROUP_NBLOCK);
            for (size_t j = 0; j < nblock; ++j) {
                Block* b = bg->blocks[j].load(butil::memory_order_relaxed);
                if (NULL == b) {
                    continue;
                }
                for (size_t k = 0; k < b->nitem; ++k) {
                    T* const objs = (T*)b->items;
                    objs[k].~T();
                }
                delete b;
            }
            delete bg;
        }

        // 初始化"内存块分组"的指针为 NULL
        memset(_block_groups, 0, sizeof(BlockGroup*) * RP_MAX_BLOCK_NGROUP);
#endif
    }

private:
    // 从动态空闲"资源标识符"中获取所有节点信息拷贝到 |c|（"空闲资源标识符"链表）中。
    bool pop_free_chunk(FreeChunk& c) {
        // Critical for the case that most return_object are called in
        // different threads of get_object.
        // 
        // 动态空闲"资源标识符"链表为空
        if (_free_chunks.empty()) {
            return false;
        }
        pthread_mutex_lock(&_free_chunks_mutex);
        if (_free_chunks.empty()) {
            pthread_mutex_unlock(&_free_chunks_mutex);
            return false;
        }
        // 获取并删除队列尾部的空闲"资源标识符"节点。
        DynamicFreeChunk* p = _free_chunks.back();
        _free_chunks.pop_back();
        pthread_mutex_unlock(&_free_chunks_mutex);
        c.nfree = p->nfree;
        memcpy(c.ids, p->ids, sizeof(*p->ids) * p->nfree);
        free(p);
        return true;
    }

    // 将本地当前"空闲资源标识"链表 |c| 压入动态"空闲资源标识"链表内存中 |_free_chunks|
    bool push_free_chunk(const FreeChunk& c) {
        // 申请 c.nfree 个 ResourceId 内存： sizeof(*c.ids) * c.nfree
        DynamicFreeChunk* p = (DynamicFreeChunk*)malloc(
            offsetof(DynamicFreeChunk, ids) + sizeof(*c.ids) * c.nfree);
        if (!p) {
            return false;
        }
        // 拷贝"空闲资源标识"节点 |c| 中数据到 _free_chunks 一个节点中
        p->nfree = c.nfree;
        memcpy(p->ids, c.ids, sizeof(*c.ids) * c.nfree);
        pthread_mutex_lock(&_free_chunks_mutex);
        _free_chunks.push_back(p);
        pthread_mutex_unlock(&_free_chunks_mutex);
        return true;
    }
    
    // ResourcePool 单例对象指针（进程级单例）
    static butil::static_atomic<ResourcePool*> _singleton;
    static pthread_mutex_t _singleton_mutex;

    // 每个 ResourcePool 对象，在同一个线程下，只有一个 LocalPool 对象（线程级单例）
    static BAIDU_THREAD_LOCAL LocalPool* _local_pool;
    
    // 记录创建 LocalPool 个数（其实也能反应出分配线程个数）
    static butil::static_atomic<long> _nlocal;
    
    // 已分配 BlockGroup 个数
    static butil::static_atomic<size_t> _ngroup;
    static pthread_mutex_t _block_group_mutex;

    static pthread_mutex_t _change_thread_mutex;
    
    // BlockGroup 数组
    static butil::static_atomic<BlockGroup*> _block_groups[RP_MAX_BLOCK_NGROUP];

    // 动态空闲"资源标识符"节点数组（柔性大小）。只有当 LocalPool::_cur_free 不足时，才
    // 使用该队列
    std::vector<DynamicFreeChunk*> _free_chunks;
    pthread_mutex_t _free_chunks_mutex;

    // 全局空闲"资源标识符"个数
#ifdef BUTIL_RESOURCE_POOL_NEED_FREE_ITEM_NUM
    static butil::static_atomic<size_t> _global_nfree;
#endif
};

// Declare template static variables:

template <typename T>
const size_t ResourcePool<T>::FREE_CHUNK_NITEM;

template <typename T>
BAIDU_THREAD_LOCAL typename ResourcePool<T>::LocalPool*
ResourcePool<T>::_local_pool = NULL;

template <typename T>
butil::static_atomic<ResourcePool<T>*> ResourcePool<T>::_singleton =
    BUTIL_STATIC_ATOMIC_INIT(NULL);

template <typename T>
pthread_mutex_t ResourcePool<T>::_singleton_mutex = PTHREAD_MUTEX_INITIALIZER;

template <typename T>
butil::static_atomic<long> ResourcePool<T>::_nlocal = BUTIL_STATIC_ATOMIC_INIT(0);

template <typename T>
butil::static_atomic<size_t> ResourcePool<T>::_ngroup = BUTIL_STATIC_ATOMIC_INIT(0);

template <typename T>
pthread_mutex_t ResourcePool<T>::_block_group_mutex = PTHREAD_MUTEX_INITIALIZER;

template <typename T>
pthread_mutex_t ResourcePool<T>::_change_thread_mutex =
    PTHREAD_MUTEX_INITIALIZER;

template <typename T>
butil::static_atomic<typename ResourcePool<T>::BlockGroup*>
ResourcePool<T>::_block_groups[RP_MAX_BLOCK_NGROUP] = {};

#ifdef BUTIL_RESOURCE_POOL_NEED_FREE_ITEM_NUM
template <typename T>
butil::static_atomic<size_t> ResourcePool<T>::_global_nfree = BUTIL_STATIC_ATOMIC_INIT(0);
#endif

template <typename T>
inline bool operator==(ResourceId<T> id1, ResourceId<T> id2) {
    return id1.value == id2.value;
}

template <typename T>
inline bool operator!=(ResourceId<T> id1, ResourceId<T> id2) {
    return id1.value != id2.value;
}

// Disable comparisons between different typed ResourceId
template <typename T1, typename T2>
bool operator==(ResourceId<T1> id1, ResourceId<T2> id2);

template <typename T1, typename T2>
bool operator!=(ResourceId<T1> id1, ResourceId<T2> id2);

inline std::ostream& operator<<(std::ostream& os,
                                ResourcePoolInfo const& info) {
    return os << "local_pool_num: " << info.local_pool_num
              << "\nblock_group_num: " << info.block_group_num
              << "\nblock_num: " << info.block_num
              << "\nitem_num: " << info.item_num
              << "\nblock_item_num: " << info.block_item_num
              << "\nfree_chunk_item_num: " << info.free_chunk_item_num
              << "\ntotal_size: " << info.total_size;
#ifdef BUTIL_RESOURCE_POOL_NEED_FREE_ITEM_NUM
              << "\nfree_num: " << info.free_item_num
#endif
           ;
}

}  // namespace butil

#endif  // BUTIL_RESOURCE_POOL_INL_H

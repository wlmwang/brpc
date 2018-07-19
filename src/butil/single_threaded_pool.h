// Copyright (c) 2011 Baidu, Inc.
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
// Date: Mon. Nov 7 14:47:36 CST 2011

#ifndef BUTIL_SINGLE_THREADED_POOL_H
#define BUTIL_SINGLE_THREADED_POOL_H

#include <stdlib.h>   // malloc & free

namespace butil {

// A single-threaded pool for very efficient allocations of same-sized items.
// Example:
//   SingleThreadedPool<16, 512> pool;
//   void* mem = pool.get();
//   pool.back(mem);
// 
//     
// 单线程环境的内存池实现，可以非常有效地分配相同大小的内存。 ResourcePool 的单线程版本。
// 
// Example:
//   SingleThreadedPool<16, 512> pool;
//   void* mem = pool.get();    // 获取 16 字节内存
//   pool.back(mem);    // 加入空闲节点链表
//   

template <size_t ITEM_SIZE_IN,   // size of an item 
          // SingleThreadedPool::get() 内存分配单位
          
          size_t BLOCK_SIZE_IN,  // suggested size of a block
          // 单个 block 大小

          size_t MIN_NITEM = 1>  // minimum number of items in one block
          // 单个 block 最少能容纳多个"项目"（内存分配单位）

class SingleThreadedPool {
public:
    // Note: this is a union. The next pointer is set iff when spaces is free,
    // ok to be overlapped.
    // 
    // "项目"的联合体。单个内存分配单位结构。当分配的某个 Node 内存空间作为空闲节点插入
    // 空闲链表时，复用 next 指针。
    union Node {
        Node* next;
        char spaces[ITEM_SIZE_IN];
    };
    struct Block {
        // 一个内存块，实际能被分配的字节大小。减去 |nalloc| 和 |next| 字段大小。
        static const size_t INUSE_SIZE =
            BLOCK_SIZE_IN - sizeof(void*) - sizeof(size_t);

        // 一个内存块，实际容纳"项目"的数量
        static const size_t NITEM = (sizeof(Node) <= INUSE_SIZE ?
                                     (INUSE_SIZE / sizeof(Node)) : MIN_NITEM);
        
        // 当前内存块的 nodes 内存池实际被分配出去的数量
        size_t nalloc;

        // 内存块链表
        Block* next;

        // 项目内存池
        Node nodes[NITEM];
    };
    // 一个内存块大小
    static const size_t BLOCK_SIZE = sizeof(Block);
    // 一个内存块，实际容纳"项目"的数量
    static const size_t NITEM = Block::NITEM;
    // 一个"项目"大小
    static const size_t ITEM_SIZE = ITEM_SIZE_IN;
    
    SingleThreadedPool() : _free_nodes(NULL), _blocks(NULL) {}
    ~SingleThreadedPool() { reset(); }

    void swap(SingleThreadedPool & other) {
        std::swap(_free_nodes, other._free_nodes);
        std::swap(_blocks, other._blocks);
    }

    // Get space of an item. The space is as long as ITEM_SIZE.
    // Returns NULL on out of memory
    // 
    // 获取一个大小为 ITEM_SIZE_IN 的内存空间。 OOM 时，返回 NULL
    void* get() {
        if (_free_nodes) {
            // 分配空闲节点内存空间（使用 back() 加入的节点）
            void* spaces = _free_nodes->spaces;
            _free_nodes = _free_nodes->next;
            return spaces;
        }
        // 内存块为空，或者分配数量超过单块能容纳"项目"的数量，创建新的内存池
        if (_blocks == NULL || _blocks->nalloc >= Block::NITEM) {
            Block* new_block = (Block*)malloc(sizeof(Block));
            if (new_block == NULL) {
                return NULL;
            }
            new_block->nalloc = 0;
            // 将新申请的内存块插入链表头部
            new_block->next = _blocks;
            _blocks = new_block;
        }
        // 分配内存空间
        return _blocks->nodes[_blocks->nalloc++].spaces;
    }
    
    // Return a space allocated by get() before.
    // Do nothing for NULL.
    // 
    // 将之前由 get() 分配的空间。
    void back(void* p) {
        if (NULL != p) {
            // 计算 |p| 节点头部地址（ |p| 为指向 Node::spaces 的指针）。
            Node* node = (Node*)((char*)p - offsetof(Node, spaces));
            // 将 |p| 指针所在的 Node 节点插入空闲节点头部
            node->next = _free_nodes;
            _free_nodes = node;
        }
    }

    // Remove all allocated spaces. Spaces that are not back()-ed yet become
    // invalid as well.
    void reset() {
        _free_nodes = NULL;
        while (_blocks) {
            Block* next = _blocks->next;
            free(_blocks);
            _blocks = next;
        }
    }

    // Count number of allocated/free/actively-used items.
    // Notice that these functions walk through all free nodes or blocks and
    // are not O(1).
    size_t count_allocated() const {
        size_t n = 0;
        for (Block* p = _blocks; p; p = p->next) {
            n += p->nalloc;
        }
        return n;
    }
    size_t count_free() const {
        size_t n = 0;
        for (Node* p = _free_nodes; p; p = p->next, ++n) {}
        return n;
    }
    size_t count_active() const {
        return count_allocated() - count_free();
    }

private:
    // You should not copy a pool.
    // 
    // 禁止拷贝复制
    SingleThreadedPool(const SingleThreadedPool&);
    void operator=(const SingleThreadedPool&);

    Node* _free_nodes;
    Block* _blocks;
};

}  // namespace butil

#endif  // BUTIL_SINGLE_THREADED_POOL_H

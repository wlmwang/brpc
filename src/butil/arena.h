// Copyright (c) 2015 Baidu, Inc.
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
// Date: Fri Jun  5 18:25:40 CST 2015

// Do small memory allocations on continuous blocks.
// 
// 高效的在连续内存块上执行小内存分配的简单内存池。只分配内存，不负责释放内存（进程结
// 束或手动释放 Arena 对象，才会释放所有内存）。

#ifndef BUTIL_ARENA_H
#define BUTIL_ARENA_H

#include <stdint.h>
#include "butil/macros.h"

namespace butil {

// Arena 构造配置选项
struct ArenaOptions {
    size_t initial_block_size;  // 初始块字节数(64 byte)
    size_t max_block_size;      // 块最大字节数(8M byte)

    // Constructed with default options.
    ArenaOptions();
};

// Just a proof-of-concept, will be refactored in future CI.
// 
// 只分配内存，不负责释放内存（进程结束或手动释放 Arena 对象，才会释放所有内存）
class Arena {
public:
    explicit Arena(const ArenaOptions& options = ArenaOptions());
    // 释放所有内存资源
    ~Arena();
    // swap 内存池
    void swap(Arena&);
    // 申请分配 n 字节内存
    void* allocate(size_t n);
    // 对齐方式申请分配 n 字节内存
    void* allocate_aligned(size_t n);  // not implemented.
    // 释放内存池中所有内存资源
    void clear();

private:
    DISALLOW_COPY_AND_ASSIGN(Arena);

    struct Block {
        uint32_t left_space() const { return size - alloc_size; }
        
        Block* next;            // 下一个块指针（组成单链表）
        uint32_t alloc_size;    // 当前块已分配了内存大小
        uint32_t size;          // 当前块总容量
        char data[0];           // 内存块地址（柔性数组）
    };

    // 申请分配一个新的块
    void* allocate_in_other_blocks(size_t n);
    void* allocate_new_block(size_t n);
    // 弹出头块，并返回指针。 |head| 为链表头节点
    Block* pop_block(Block* & head) {
        Block* saved_head = head;
        head = head->next;
        return saved_head;
    }
    
    Block* _cur_block;          // 当前正在分配的块的指针
    Block* _isolated_blocks;    // 已完成分配任务块的首块指针
    size_t _block_size;         // 下一次申请 Block 块最小字节（每次提升 2 倍）
    ArenaOptions _options;      // Arena 配置选项
};

// 申请分配 n 字节内存
inline void* Arena::allocate(size_t n) {
    if (_cur_block != NULL && _cur_block->left_space() >= n) {
        // 直接从已申请内存块上分配小内存给调用者
        void* ret = _cur_block->data + _cur_block->alloc_size;
        _cur_block->alloc_size += n;
        return ret;
    }
    // 内存池内存剩余不足，申请新的内存块（内存资源）
    return allocate_in_other_blocks(n);
}

}  // namespace butil

#endif  // BUTIL_ARENA_H

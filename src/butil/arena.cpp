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

#include <stdlib.h>
#include <algorithm>
#include "butil/arena.h"

namespace butil {

ArenaOptions::ArenaOptions()
    : initial_block_size(64)
    , max_block_size(8192)
{}

Arena::Arena(const ArenaOptions& options)
    : _cur_block(NULL)
    , _isolated_blocks(NULL)
    , _block_size(options.initial_block_size)
    , _options(options) {
}

// 释放所有块上内存资源
Arena::~Arena() {
    while (_cur_block != NULL) {
        Block* const saved_next = _cur_block->next;
        free(_cur_block);
        _cur_block = saved_next;
    }
    while (_isolated_blocks != NULL) {
        Block* const saved_next = _isolated_blocks->next;
        free(_isolated_blocks);
        _isolated_blocks = saved_next;
    }
}

// swap 内存池
void Arena::swap(Arena& other) {
    std::swap(_cur_block, other._cur_block);
    std::swap(_isolated_blocks, other._isolated_blocks);
    std::swap(_block_size, other._block_size);
    const ArenaOptions tmp = _options;
    _options = other._options;
    other._options = tmp;
}

void Arena::clear() {
    // TODO(gejun): Reuse memory
    // 
    // 释放内存池中所有内存资源（触发析构）
    Arena a;
    swap(a);
}

// 申请内存块并分配之，随即将其放入 _isolated_blocks 链表
void* Arena::allocate_new_block(size_t n) {
    Block* b = (Block*)malloc(offsetof(Block, data) + n);
    b->next = _isolated_blocks;
    b->alloc_size = n;
    b->size = n;
    _isolated_blocks = b;
    return b->data;
}

void* Arena::allocate_in_other_blocks(size_t n) {
    if (n > _block_size / 4) { // put outlier on separate blocks.
        // 申请分配单独内存块，并直接返回
        return allocate_new_block(n);
    }
    // Waste the left space. At most 1/4 of allocated spaces are wasted.
    // 
    // 剩余空间被浪费，最大浪费为当前块 1/4 的空间

    // Grow the block size gradually.
    // 
    // 逐渐增加分配内存块大小，最大 max_block_size(8M)
    if (_cur_block != NULL) {
        _block_size = std::min(2 * _block_size, _options.max_block_size);
    }
    // 申请 new_size 大小内存块(最小为 n)
    size_t new_size = _block_size;
    if (new_size < n) {
        new_size = n;
    }
    // offsetof(Block, data) + new_size 表示申请长度是不包含 Block 大小内存块的长度
    Block* b = (Block*)malloc(offsetof(Block, data) + new_size);
    if (NULL == b) {
        return NULL;
    }
    b->next = NULL;
    b->alloc_size = n;  // 当前块已分配了内存大小
    b->size = new_size; // 当前块总容量
    if (_cur_block) {
        // 将原始 _cur_block 插入 _isolated_blocks 头，以便让 _cur_block 指向新的
        // 当前块节点
        _cur_block->next = _isolated_blocks;
        _isolated_blocks = _cur_block;
    }
    // 让 _cur_block 指向新的当前块节点
    _cur_block = b;
    return b->data;
}

}  // namespace butil

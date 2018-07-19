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
// Date: Tue Feb 25 23:43:39 CST 2014

// Provide functions to get/set bits of an integral array. These functions
// are not threadsafe because operations on different bits may modify a same
// integer.
// 
// 提供函数来 get/set 整型数组的位运算。这些函数不是线程安全的，因为对不同位的操作可能
// 会修改相同的整数.

#ifndef BUTIL_BIT_ARRAY_H
#define BUTIL_BIT_ARRAY_H

#include <stdint.h>

namespace butil {

// Create an array with at least |nbit| bits. The array is not cleared.
// 
// 创建一个至少可容纳 |nbit| 位的 uint64_t 类型的数组。
inline uint64_t* bit_array_malloc(size_t nbit)
{
    if (!nbit) {
        return NULL;
    }
    // 注：malloc 是以字节为单位的函数（乘以 8 表示一个 uint64_t 长度为 8 字节）
    return (uint64_t*)malloc((nbit + 63 ) / 64 * 8/*different from /8*/);
}

// Set bit 0 ~ nbit-1 of |array| to be 0
// 
// 0 ~ nbit-1 位的 |array| 清零
inline void bit_array_clear(uint64_t* array, size_t nbit)
{
    // 注：memset 是以字节为单位的函数，故将最后 6 位所指的 bit 位置单独置 0
    const size_t off = (nbit >> 6);
    memset(array, 0, off * 8);
    const size_t last = (off << 6);
    if (last != nbit) {
        // ~((((uint64_t)1) << (nbit - last)) - 1)  # 前 nbit - last - 1 位全部置为 0
        array[off] &= ~((((uint64_t)1) << (nbit - last)) - 1);
    }
}

// Set i-th bit (from left, counting from 0) of |array| to be 1
// 
// 设置 |array| 的第 i 位为 1 （左计数，0 开始）
inline void bit_array_set(uint64_t* array, size_t i)
{
    const size_t off = (i >> 6);
    array[off] |= (((uint64_t)1) << (i - (off << 6)));
}

// Set i-th bit (from left, counting from 0) of |array| to be 0
// 
// 设置 |array| 的第 i 位为 0 （左计数，0 开始）
inline void bit_array_unset(uint64_t* array, size_t i)
{
    const size_t off = (i >> 6);
    array[off] &= ~(((uint64_t)1) << (i - (off << 6)));
}

// Get i-th bit (from left, counting from 0) of |array|
// 
// 获取 |array| 的第 i 位的值 （左计数，0 开始）
inline uint64_t bit_array_get(const uint64_t* array, size_t i)
{
    const size_t off = (i >> 6);
    return (array[off] & (((uint64_t)1) << (i - (off << 6))));
}

// Find index of first 1-bit from bit |begin| to |end| in |array|.
// Returns |end| if all bits are 0.
// This function is of O(nbit) complexity.
// 
// @tips
// int __builtin_ctzl (unsigned long x);
// 返回从最低有效位位置开始的 x 中的尾随 0 位数。如果 x 为 0 ，则结果为未
// 定义。
// 
// 
// 在 |array| 中，从 |begin| 到 |end| 里寻找第一个为 1 的位的索引值。如
// 果所有位都为 0，返回 |end| 。该函数是 O(nbit) 复杂度。
inline size_t bit_array_first1(const uint64_t* array, size_t begin, size_t end)
{
    size_t off1 = (begin >> 6);
    const size_t first = (off1 << 6);
    if (first != begin) {
        const uint64_t v =
            array[off1] & ~((((uint64_t)1) << (begin - first)) - 1);
        if (v) {
            return std::min(first + __builtin_ctzl(v), end);
        }
        ++off1;
    }
    
    const size_t off2 = (end >> 6);
    for (size_t i = off1; i < off2; ++i) {
        if (array[i]) {
            return i * 64 + __builtin_ctzl(array[i]);
        }
    }
    const size_t last = (off2 << 6);
    if (last != end && array[off2]) {
        return std::min(last + __builtin_ctzl(array[off2]), end);
    }
    return end;
}

}  // end namespace butil

#endif  // BUTIL_BIT_ARRAY_H

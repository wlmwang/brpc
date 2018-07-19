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
// Date: Thu Dec 31 13:35:39 CST 2015

#ifndef BUTIL_FAST_RAND_H
#define BUTIL_FAST_RAND_H

#include <stdint.h>

namespace butil {

// Generate random values fast without global contentions.
// All functions in this header are thread-safe.
// 
// 快速随机数生成器（无需全局竞争），所有函数线程安全。

// 随机数种子结构（数组的两个元素进行一系列异或、移位生成一个随机数）
struct FastRandSeed {
    uint64_t s[2];
};

// Initialize the seed.
// 
// 初始化随机数种子。
void init_fast_rand_seed(FastRandSeed* seed);

// Generate an unsigned 64-bit random number from thread-local or given seed.
// Cost: ~5ns
// 
// 从 thread-local 全局种子变量或给定的种子生成一个无符号的 64 位随机数。
uint64_t fast_rand();
uint64_t fast_rand(FastRandSeed*);

// Generate an unsigned 64-bit random number inside [0, range) from
// thread-local seed.
// Returns 0 when range is 0.
// Cost: ~30ns
// Note that this can be used as an adapter for std::random_shuffle():
//   std::random_shuffle(myvector.begin(), myvector.end(), butil::fast_rand_less_than);
//   
// 根据 thread-local 全局的种子变量，生成 unsigned 64-bit 介于 [0, range) 的随机数。
uint64_t fast_rand_less_than(uint64_t range);

// Generate a 64-bit random number inside [min, max] (inclusive!)
// from thread-local seed.
// NOTE: this function needs to be a template to be overloadable properly.
// Cost: ~30ns
// 
// 根据 thread-local 全局的种子变量，生成 64-bit 介于 [min, max] 的随机数。
template <typename T> T fast_rand_in(T min, T max) {
    extern int64_t fast_rand_in_64(int64_t min, int64_t max);
    extern uint64_t fast_rand_in_u64(uint64_t min, uint64_t max);
    if ((T)-1 < 0) {
    	// 有符号（转换成 64-bit）
        return fast_rand_in_64((int64_t)min, (int64_t)max);
    } else {
    	// 无符号（转换成 64-bit）
        return fast_rand_in_u64((uint64_t)min, (uint64_t)max);
    }
}

// Generate a random double in [0, 1) from thread-local seed.
// Cost: ~15ns
// 
// 根据 thread-local 全局的种子变量，生成 double 介于 [0, 1) 的随机数。
double fast_rand_double();

}

#endif  // BUTIL_FAST_RAND_H

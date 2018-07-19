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

#include <limits>          // numeric_limits
#include <math.h>
#include "butil/basictypes.h"
#include "butil/macros.h"
#include "butil/time.h"     // gettimeofday_us()
#include "butil/fast_rand.h"

namespace butil {

// This state can be seeded with any value.
// 
// 64-bit 随机种子状态
typedef uint64_t SplitMix64Seed;

// A very fast generator passing BigCrush. Due to its 64-bit state, it's
// solely for seeding xorshift128+.
// 
// 基于异或、移位（“分裂融合”）操作生成一个“随机”（不可逆）整型值。
inline uint64_t splitmix64_next(SplitMix64Seed* seed) {
    uint64_t z = (*seed += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

// xorshift128+ is the fastest generator passing BigCrush without systematic
// failures
// 
// xorshift128+ 是通过 BigCrush 没有系统故障的最快的随机数种子发生器。
inline uint64_t xorshift128_next(FastRandSeed* seed) {
    uint64_t s1 = seed->s[0];
    const uint64_t s0 = seed->s[1];
    seed->s[0] = s0;
    s1 ^= s1 << 23; // a
    seed->s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5); // b, c
    return seed->s[1] + s0;
}

// seed xorshift128+ with splitmix64.
// 
// 使用 splitmix64_next 初始化随机种子（当前微妙时间戳为 splitmix64_next 的种子）。
void init_fast_rand_seed(FastRandSeed* seed) {
    SplitMix64Seed seed4seed = butil::gettimeofday_us();
    seed->s[0] = splitmix64_next(&seed4seed);
    seed->s[1] = splitmix64_next(&seed4seed);
}

inline uint64_t fast_rand_impl(uint64_t range, FastRandSeed* seed) {
    // Separating uint64_t values into following intervals:
    //   [0,range-1][range,range*2-1] ... [uint64_max/range*range,uint64_max]
    // If the generated 64-bit random value falls into any interval except the
    // last one, the probability of taking any value inside [0, range-1] is
    // same. If the value falls into last interval, we retry the process until
    // the value falls into other intervals. If min/max are limited to 32-bits,
    // the retrying is rare. The amortized retrying count at maximum is 1 when 
    // range equals 2^32. A corner case is that even if the range is power of
    // 2(e.g. min=0 max=65535) in which case the retrying can be avoided, we
    // still retry currently. The reason is just to keep the code simpler
    // and faster for most cases.
    // 
    // 将 uint64_t 值分为以下间隔：[0,range-1][range,range*2-1] ... [uint64_max/
    // range*range,uint64_max] 。如果生成的 64 位随机值落在任意一个间隔内（除了最后一
    // 个），则它和在 [0,range-1] 内取任意值的概率是相同的。如果值落在最后一个时间间隔
    // 内，我们重试进程，直到该值落入其他间隔。
    // 
    // 分隔区间：能非常巧妙的减少循环次数（只有落在最后一个区间才会再次尝试随机）。
    const uint64_t div = std::numeric_limits<uint64_t>::max() / range;
    uint64_t result;
    do {
        result = xorshift128_next(seed) / div;
    } while (result >= range);
    return result;
}

// Seeds for different threads are stored separately in thread-local storage.
// 
// 不同线程的种子分别存储在 thread-local 线程本地存储中
static __thread FastRandSeed _tls_seed = { { 0, 0 } };

// True if the seed is (probably) uninitialized. There's definitely false
// positive, but it's OK for us.
// 
// 如果种子（可能）未初始化，则为 true。
inline bool need_init(const FastRandSeed& seed) {
    return seed.s[0] == 0 && seed.s[1] == 0;
}

uint64_t fast_rand() {
    if (need_init(_tls_seed)) {
        init_fast_rand_seed(&_tls_seed);
    }
    return xorshift128_next(&_tls_seed);
}

uint64_t fast_rand(FastRandSeed* seed) {
    return xorshift128_next(seed);
}

uint64_t fast_rand_less_than(uint64_t range) {
    if (range == 0) {
        return 0;
    }
    if (need_init(_tls_seed)) {
        init_fast_rand_seed(&_tls_seed);
    }
    return fast_rand_impl(range, &_tls_seed);
}

int64_t fast_rand_in_64(int64_t min, int64_t max) {
    if (need_init(_tls_seed)) {
        init_fast_rand_seed(&_tls_seed);
    }
    if (min >= max) {
        if (min == max) {
            return min;
        }
        const int64_t tmp = min;
        min = max;
        max = tmp;
    }
    int64_t range = max - min + 1;
    if (range == 0) {
        // max = INT64_MAX, min = INT64_MIN
        return (int64_t)xorshift128_next(&_tls_seed);
    }
    return min + (int64_t)fast_rand_impl(max - min + 1, &_tls_seed);
}

uint64_t fast_rand_in_u64(uint64_t min, uint64_t max) {
    if (need_init(_tls_seed)) {
        init_fast_rand_seed(&_tls_seed);
    }
    if (min >= max) {
        if (min == max) {
            return min;
        }
        const uint64_t tmp = min;
        min = max;
        max = tmp;
    }
    uint64_t range = max - min + 1;
    if (range == 0) {
        // max = UINT64_MAX, min = UINT64_MIN
        return xorshift128_next(&_tls_seed);
    }
    return min + fast_rand_impl(range, &_tls_seed);
}

inline double fast_rand_double(FastRandSeed* seed) {
    // Copied from rand_util.cc
    // 
    // @tips
    // ::radix 的值用于表示系统的一个数字类型的底。对于所有二进制数值类型为 2 。它可以
    // 是别的值，譬如对 IEEE 754 十进制浮点类型或第三方二进制编码十进制整数为 10 。
    // 
    // ::digits 的值是能无更改地表示类型 T 的 radix 底位数。对于整数类型，这是不含符号
    // 位和填充位（若存在）的位数。对于浮点类型，这是尾数的位数。
    // 
    // 
    // 我们尝试通过屏蔽掉目标类型尾数的多个位来获得最大精度，并将其提高到适当的倍数以产
    // 生 [0,1] 范围内的输出。对于 IEEE 754 双精度，尾数预计可容纳 53 位。

    COMPILE_ASSERT(std::numeric_limits<double>::radix == 2, otherwise_use_scalbn);
    static const int kBits = std::numeric_limits<double>::digits;
    // 保留此次随机数的尾数位数的二进制值。
    uint64_t random_bits = xorshift128_next(seed) & ((UINT64_C(1) << kBits) - 1);
    // @tips
    // \file <math.h>
    // double ldexp(double x, int exp);
    // 用来求一个数乘上 2 的 exp 次方的值。返回值为 x * 2^exp
    double result = ldexp(static_cast<double>(random_bits), -1 * kBits);
    return result;
}

double fast_rand_double() {
    if (need_init(_tls_seed)) {
        init_fast_rand_seed(&_tls_seed);
    }
    return fast_rand_double(&_tls_seed);
}

}  // namespace butil

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_RAND_UTIL_H_
#define BUTIL_RAND_UTIL_H_

#include <string>

#include "butil/base_export.h"
#include "butil/basictypes.h"

namespace butil {

// --------------------------------------------------------------------------
// NOTE(gejun): Functions in this header read from /dev/urandom in posix
// systems and are not proper for performance critical situations.
// For fast random numbers, check fast_rand.h
// --------------------------------------------------------------------------
// 
// 这个头文件中的函数在 posix 系统中，将从 /dev/urandom 中读取随机字节流，并不适用于性
// 能要求很高场景。对于快速随机数，请检查 <fast_rand.h>
// 
// @tips
// 在 UNIX 操作系统（包括类 UNIX 系统）中， /dev/random 是一个特殊的设备文件，可以用作
// 随机数发生器或伪随机数发生器。原理：是利用当前系统的熵池来计算出一定数量的随机比特，然
// 后将这些比特作为字节流返回。熵池就是当前系统的环境噪音，熵指的是一个系统的混乱程度，系
// 统噪音可以通过很多参数来评估，如内存的使用，文件的使用量，不同类型的进程数量等等。
// 
// /dev/random 的一个副本是 /dev/urandom ("unblocked"，非阻塞的随机数发生器)

// Returns a random number in range [0, kuint64max]. Thread-safe.
// 
// 返回范围 [0, kuint64max] 中的随机数。线程安全。
BUTIL_EXPORT uint64_t RandUint64();

// Returns a random number between min and max (inclusive). Thread-safe.
// 
// 返回 [min, max] 之间随机数。线程安全。
BUTIL_EXPORT int RandInt(int min, int max);

// Returns a random number in range [0, range).  Thread-safe.
//
// Note that this can be used as an adapter for std::random_shuffle():
// Given a pre-populated |std::vector<int> myvector|, shuffle it as
//   std::random_shuffle(myvector.begin(), myvector.end(), butil::RandGenerator);
//   
// 返回 [0, range) 之间随机数。线程安全。
// 
// 注意，这可以用作 std::random_shuffle() 适配器：给定预先填充的 |std::vector<int> 
// myvector| ，将其改为 std::random_shuffle(myvector.begin(), myvector.end(), 
// butil::RandGenerator);
BUTIL_EXPORT uint64_t RandGenerator(uint64_t range);

// Returns a random double in range [0, 1). Thread-safe.
// 
// 返回 [0, 1) 之间随机数（双精度）。线程安全。
BUTIL_EXPORT double RandDouble();

// Given input |bits|, convert with maximum precision to a double in
// the range [0, 1). Thread-safe.
BUTIL_EXPORT double BitsToOpenEndedUnitInterval(uint64_t bits);

// Fills |output_length| bytes of |output| with random data.
//
// WARNING:
// Do not use for security-sensitive purposes.
// See crypto/ for cryptographically secure random number generation APIs.
// 
// 使用 |output_length| 长度随机数据填充到 |output| 中。
BUTIL_EXPORT void RandBytes(void* output, size_t output_length);

// Fills a string of length |length| with random data and returns it.
// |length| should be nonzero.
//
// Note that this is a variation of |RandBytes| with a different return type.
// The returned string is likely not ASCII/UTF-8. Use with care.
//
// WARNING:
// Do not use for security-sensitive purposes.
// See crypto/ for cryptographically secure random number generation APIs.
BUTIL_EXPORT std::string RandBytesAsString(size_t length);

#if defined(OS_POSIX)
BUTIL_EXPORT int GetUrandomFD();
#endif

}  // namespace butil

#endif  // BUTIL_RAND_UTIL_H_

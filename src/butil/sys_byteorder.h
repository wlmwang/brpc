// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header defines cross-platform ByteSwap() implementations for 16, 32 and
// 64-bit values, and NetToHostXX() / HostToNextXX() functions equivalent to
// the traditional ntohX() and htonX() functions.
// Use the functions defined here rather than using the platform-specific
// functions directly.
// 
// 此文件定义了用于 16/32/64-bit 跨平台 ByteSwap() 实现。建议使用这些函数，而不
// 直接使用平台特定的实现。 NetToHostXX|HostToNextXX 相当于 ntohX|htonX 函数。


// @tips
// /* Licensed under LGPLv2.1+ - see LICENSE file for details */
// 
// static inline uint16_t bswap_16(uint16_t val)
// {
//   return ((val & (uint16_t)0x00ffU) << 8)
//     | ((val & (uint16_t)0xff00U) >> 8);
// }
// 
// static inline uint32_t bswap_32(uint32_t val)
// {
//   return ((val & (uint32_t)0x000000ffUL) << 24)
//     | ((val & (uint32_t)0x0000ff00UL) <<  8)
//     | ((val & (uint32_t)0x00ff0000UL) >>  8)
//     | ((val & (uint32_t)0xff000000UL) >> 24);
// }
// 
// #if !HAVE_BSWAP_64
// static inline uint64_t bswap_64(uint64_t val)
// {
//   return ((val & (uint64_t)0x00000000000000ffULL) << 56)
//     | ((val & (uint64_t)0x000000000000ff00ULL) << 40)
//     | ((val & (uint64_t)0x0000000000ff0000ULL) << 24)
//     | ((val & (uint64_t)0x00000000ff000000ULL) <<  8)
//     | ((val & (uint64_t)0x000000ff00000000ULL) >>  8)
//     | ((val & (uint64_t)0x0000ff0000000000ULL) >> 24)
//     | ((val & (uint64_t)0x00ff000000000000ULL) >> 40)
//     | ((val & (uint64_t)0xff00000000000000ULL) >> 56);
// }
// #endif


#ifndef BUTIL_SYS_BYTEORDER_H_
#define BUTIL_SYS_BYTEORDER_H_

#include "butil/basictypes.h"
#include "butil/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace butil {

#if defined(COMPILER_MSVC)
#include <stdlib.h>
inline uint16_t ByteSwap(uint16_t x) { return _byteswap_ushort(x); }
inline uint32_t ByteSwap(uint32_t x) { return _byteswap_ulong(x); }
inline uint64_t ByteSwap(uint64_t x) { return _byteswap_uint64(x); }

#elif defined(OS_MACOSX)
// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
inline uint16_t ByteSwap(uint16_t x) { return OSSwapInt16(x); }
inline uint32_t ByteSwap(uint32_t x) { return OSSwapInt32(x); }
inline uint64_t ByteSwap(uint64_t x) { return OSSwapInt64(x); }

#elif defined(OS_LINUX)
#include <byteswap.h>
inline uint16_t ByteSwap(uint16_t x) { return bswap_16(x); }
inline uint32_t ByteSwap(uint32_t x) { return bswap_32(x); }
inline uint64_t ByteSwap(uint64_t x) { return bswap_64(x); }

#else
// Returns a value with all bytes in |x| swapped, i.e. reverses the endianness.
// 
// 转换 |x| 中的字节序：按字节交换。即，所有字节反转，并返回结果。
inline uint16_t ByteSwap(uint16_t x) {
  return (x << 8) | (x >> 8);
}

inline uint32_t ByteSwap(uint32_t x) {
    x = ((x & 0xff00ff00UL) >> 8) | ((x & 0x00ff00ffUL) << 8);
    return (x >> 16) | (x << 16);
}

inline uint64_t ByteSwap(uint64_t x) {
    x = ((x & 0xff00ff00ff00ff00ULL) >> 8) | ((x & 0x00ff00ff00ff00ffULL) << 8);
    x = ((x & 0xffff0000ffff0000ULL) >> 16) | ((x & 0x0000ffff0000ffffULL) << 16);
    return (x >> 32) | (x << 32);
}
#endif

// Converts the bytes in |x| from host order (endianness) to little endian, and
// returns the result.
// 
// 转换 |x| 中的字节序：从主机序（可能是小端）到小端序，并返回结果。
inline uint16_t ByteSwapToLE16(uint16_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}
inline uint32_t ByteSwapToLE32(uint32_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}
inline uint64_t ByteSwapToLE64(uint64_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return x;
#else
  return ByteSwap(x);
#endif
}

// Converts the bytes in |x| from network to host order (endianness), and
// returns the result.
// 
// 转换 |x| 中的字节序：从网络序（大端）到主机序（可能是小段），并返回结果。
inline uint16_t NetToHost16(uint16_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint32_t NetToHost32(uint32_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint64_t NetToHost64(uint64_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}

// Converts the bytes in |x| from host to network order (endianness), and
// returns the result.
// 
// 转换 |x| 中的字节序：从主机序（可能是小端）到网络序（大端），并返回结果。
inline uint16_t HostToNet16(uint16_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint32_t HostToNet32(uint32_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}
inline uint64_t HostToNet64(uint64_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return ByteSwap(x);
#else
  return x;
#endif
}

}  // namespace butil

#endif  // BUTIL_SYS_BYTEORDER_H_

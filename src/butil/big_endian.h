// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 带大端字节序（网络序）操作缓冲区接口类

#ifndef BUTIL_BIG_ENDIAN_H_
#define BUTIL_BIG_ENDIAN_H_

#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/strings/string_piece.h"

namespace butil {

// Read an integer (signed or unsigned) from |buf| in Big Endian order.
// Note: this loop is unrolled with -O1 and above.
// NOTE(szym): glibc dns-canon.c and SpdyFrameBuilder use
// ntohs(*(uint16_t*)ptr) which is potentially unaligned.
// This would cause SIGBUS on ARMv5 or earlier and ARMv6-M.
// 
// 从 |buf| 中读取一个大端序整型值（signed or unsigned）转换写到 |out| 中。
template<typename T>
inline void ReadBigEndian(const char buf[], T* out) {
  *out = buf[0];
  for (size_t i = 1; i < sizeof(T); ++i) {
    *out <<= 8;
    // Must cast to uint8_t to avoid clobbering by sign extension.
    // 
    // 必须转换为 uint8_t 以避免有符号扩展出错。
    *out |= static_cast<uint8_t>(buf[i]);
  }
}

// Write an integer (signed or unsigned) |val| to |buf| in Big Endian order.
// Note: this loop is unrolled with -O1 and above.
// 
// T 类型整型值 |val|(signed or unsigned) 转换为大端序写到 |buf| 中。
template<typename T>
inline void WriteBigEndian(char buf[], T val) {
  for (size_t i = 0; i < sizeof(T); ++i) {
    buf[sizeof(T)-i-1] = static_cast<char>(val & 0xFF);
    val >>= 8;
  }
}

// uint8_t 版本模板特化。

// Specializations to make clang happy about the (dead code) shifts above.
template<>
inline void ReadBigEndian<uint8_t>(const char buf[], uint8_t* out) {
  *out = buf[0];
}

template<>
inline void WriteBigEndian<uint8_t>(char buf[], uint8_t val) {
  buf[0] = static_cast<char>(val);
}

// Allows reading integers in network order (big endian) while iterating over
// an underlying buffer. All the reading functions advance the internal pointer.
// 
// 当遍历一个底层缓冲区时，提供允许以网络序 (big endian) 来读取一个整数。接口含带隐式的类型
// 提升为安全类型的读取操作。
class BUTIL_EXPORT BigEndianReader {
 public:
  BigEndianReader(const char* buf, size_t len);

  const char* ptr() const { return ptr_; }
  // 剩余缓冲区
  int remaining() const { return end_ - ptr_; }

  // 跳过指定长度 buffer
  bool Skip(size_t len);
  // 读取指定长度 buffer ，并复制到内存地址 |out| 中
  bool ReadBytes(void* out, size_t len);
  // Creates a StringPiece in |out| that points to the underlying buffer.
  // 
  // 读取指定长度 buffer ，并创建 butil::StringPiece 类对象，该对象指向 buf ，
  // 长度为 len
  bool ReadPiece(butil::StringPiece* out, size_t len);
  bool ReadU8(uint8_t* value);
  bool ReadU16(uint16_t* value);
  bool ReadU32(uint32_t* value);

 private:
  // Hidden to promote type safety.
  // 
  // 带隐式的类型提升为安全类型的读取操作。
  template<typename T>
  bool Read(T* v);

  const char* ptr_; // 输入缓冲起始 buffer
  const char* end_; // 输入缓冲超尾 buffer
};

// Allows writing integers in network order (big endian) while iterating over
// an underlying buffer. All the writing functions advance the internal pointer.
// 
// 当遍历一个底层缓冲区时，提供允许以网络序 (big endian) 来写入一个整数。接口含带隐式的类型
// 提升为安全类型的读取操作。
class BUTIL_EXPORT BigEndianWriter {
 public:
  BigEndianWriter(char* buf, size_t len);

  char* ptr() const { return ptr_; }
  // 剩余缓冲区
  int remaining() const { return end_ - ptr_; }

  // 跳过指定长度 buffer
  bool Skip(size_t len);
  bool WriteBytes(const void* buf, size_t len);
  bool WriteU8(uint8_t value);
  bool WriteU16(uint16_t value);
  bool WriteU32(uint32_t value);

 private:
  // Hidden to promote type safety.
  // 
  // 带隐式的类型提升为安全类型的写入操作。
  template<typename T>
  bool Write(T v);

  char* ptr_; // 输出缓冲起始 buffer
  char* end_; // 输出缓冲超尾 buffer
};

}  // namespace butil

#endif  // BUTIL_BIG_ENDIAN_H_

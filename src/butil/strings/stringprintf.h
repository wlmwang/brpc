// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_STRINGS_STRINGPRINTF_H_
#define BUTIL_STRINGS_STRINGPRINTF_H_

#include <stdarg.h>   // va_list

#include <string>

#include "butil/base_export.h"
#include "butil/compiler_specific.h"

// @tips
// \file #include <stdarg.h>
// int vsnprintf(char *str, size_t size, const char *format, va_list ap);
// 
// 将可变参数格式化后输出到一个字符数组。即用于向 |str| 字符串中填入数据。
// 1. 执行成功，返回最终生成字符串的长度 （不包含终止符），若生成字符串的长度大于 size ，
// 则将字符串的前 size 个字符复制到 str ，同时将原串的长度返回（不包含终止符）
// 2. 执行失败，返回负值，并设置 errno

namespace butil {

// Return a C++ string given printf-like input.
// 
// printf 风格的格式化函数，返回 std::string
BUTIL_EXPORT std::string StringPrintf(const char* format, ...)
    PRINTF_FORMAT(1, 2);
// OS_ANDROID's libc does not support wchar_t, so several overloads are omitted.
// 
// OS_ANDROID libc 库不支持 wchar_t ，忽略
#if !defined(OS_ANDROID)
BUTIL_EXPORT std::wstring StringPrintf(const wchar_t* format, ...)
    WPRINTF_FORMAT(1, 2);
#endif

// Return a C++ string given vprintf-like input.
// 
// vprintf 风格的格式化函数，返回 std::string
BUTIL_EXPORT std::string StringPrintV(const char* format, va_list ap)
    PRINTF_FORMAT(1, 0);

// Store result into a supplied string and return it.
// 
// 将结果存储到提供的 |dst| 字符串中并返回
BUTIL_EXPORT const std::string& SStringPrintf(std::string* dst,
                                             const char* format, ...)
    PRINTF_FORMAT(2, 3);
// OS_ANDROID libc 库不支持 wchar_t ，忽略
#if !defined(OS_ANDROID)
BUTIL_EXPORT const std::wstring& SStringPrintf(std::wstring* dst,
                                              const wchar_t* format, ...)
    WPRINTF_FORMAT(2, 3);
#endif

// Append result to a supplied string.
// 
// 添加格式字符串到结果参数（输出参数）
BUTIL_EXPORT void StringAppendF(std::string* dst, const char* format, ...)
    PRINTF_FORMAT(2, 3);
// OS_ANDROID libc 库不支持 wchar_t ，忽略
#if !defined(OS_ANDROID)
// TODO(evanm): this is only used in a few places in the code;
// replace with string16 version.
BUTIL_EXPORT void StringAppendF(std::wstring* dst, const wchar_t* format, ...)
    WPRINTF_FORMAT(2, 3);
#endif

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
// 
// 采用 va_list 并附加到指定字符串的底层函数。所有其他函数都只是它的便利包装器
BUTIL_EXPORT void StringAppendV(std::string* dst, const char* format, va_list ap)
    PRINTF_FORMAT(2, 0);
// OS_ANDROID libc 库不支持 wchar_t ，忽略    
#if !defined(OS_ANDROID)
BUTIL_EXPORT void StringAppendV(std::wstring* dst,
                               const wchar_t* format, va_list ap)
    WPRINTF_FORMAT(2, 0);
#endif

}  // namespace butil

#endif  // BUTIL_STRINGS_STRINGPRINTF_H_

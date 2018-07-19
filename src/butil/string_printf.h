// Copyright (c) 2011 Baidu, Inc.
// Date: Mon. Nov 7 14:47:36 CST 2011

#ifndef BUTIL_STRING_PRINTF_H
#define BUTIL_STRING_PRINTF_H

#include <string>                                // std::string
#include <stdarg.h>                              // va_list

// @tips
// __attribute__((format(printf, m, n)))
// GNU C 特色之一。其中 printf 还可以是 `scanf/strftime/strfmon` 等。其作用是：让编译器按
// 照 printf/scanf/strftime/strfmon 的参数表格式规则对该函数的参数进行检查。即决定参数是哪种
// 风格。编译器会检查函数声明和函数实际调用参数之间的格式化字符串是否匹配。该功能十分有用，尤其是处
// 理一些很难发现的 bug 。
// 
// |m| 为第几个参数为格式化字符串 (format string) （顺序从 1 开始）。
// |n| 为参数集合中的第一个，即参数 "..." 里的第一个参数在函数参数总数排在第几（顺序从 1 开始）。
// 
// Use like:
// extern void myprint(int x，const char *format,...) __attribute__((format(printf, 
// 2, 3)));
// 
// 特别注意的是，如果 myprint 是一个类成员函数，那么 |m| 和 |n| 的值统一要加 +1 。因为类成员方
// 法第一个参数实际上一个 "隐式" 的 "this" 指针。

namespace butil {

// Convert |format| and associated arguments to std::string
std::string string_printf(const char* format, ...)
    __attribute__ ((format (printf, 1, 2)));

// Write |format| and associated arguments into |output|
// Returns 0 on success, -1 otherwise.
int string_printf(std::string* output, const char* fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

// Write |format| and associated arguments in form of va_list into |output|.
// Returns 0 on success, -1 otherwise.
int string_vprintf(std::string* output, const char* format, va_list args);

// Append |format| and associated arguments to |output|
// Returns 0 on success, -1 otherwise.
int string_appendf(std::string* output, const char* format, ...)
    __attribute__ ((format (printf, 2, 3)));

// Append |format| and associated arguments in form of va_list to |output|.
// Returns 0 on success, -1 otherwise.
int string_vappendf(std::string* output, const char* format, va_list args);


}  // namespace butil

#endif  // BUTIL_STRING_PRINTF_H

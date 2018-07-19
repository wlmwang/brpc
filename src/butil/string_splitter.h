// Copyright (c) 2011 Baidu, Inc.
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
// Date: Mon. Apr. 18 19:52:34 CST 2011

// Iteratively split a string by one or multiple separators.
// 
// 通过一个或多个分隔符，迭代地分割一个字符串。

#ifndef BUTIL_STRING_SPLITTER_H
#define BUTIL_STRING_SPLITTER_H

#include <stdlib.h>
#include <stdint.h>

// It's common to encode data into strings separated by special characters
// and decode them back, but functions such as `split_string' has to modify
// the input string, which is bad. If we parse the string from scratch, the
// code will be filled with pointer operations and obscure to understand.
//
// What we want is:
// - Scan the string once: just do simple things efficiently.
// - Do not modify input string: Changing input is bad, it may bring hidden
//   bugs, concurrency issues and non-const propagations.
// - Split the string in-place without additional buffer/array.
//
// StringSplitter does meet these requirements.
// Usage:
//     const char* the_string_to_split = ...;
//     for (StringSplitter s(the_string_to_split, '\t'); s; ++s) {
//         printf("%*s\n", s.length(), s.field());    
//     }
// 
// "s" behaves as an iterator and evaluates to true before ending.
// "s.field()" and "s.length()" are address and length of current field
// respectively. Notice that "s.field()" may not end with '\0' because
// we don't modify input. You can copy the field to a dedicated buffer
// or apply a function supporting length.
// 
// 
// 将由特殊字符分隔的编码的数据字符串，进行解码。但是像 `split_string' 等函数都必须
// 修改输入字符串，这是不好的。如果我们从头开始解析字符串，代码将被填充一些指针操作，这
// 也会让代码很难懂。
// 
// 这个函数最好应该是：
// - 扫描字符串一次：只是有效地做简单的事情。
// - 不要修改输入字符串：不改变输入，它可能带来隐藏的错误，并发问题和非 const 参数问题。
// - 在没有额外缓冲区/数组的情况下就地分割字符串。
//
// Usage:
//     const char* the_string_to_split = ...;
//     for (StringSplitter s(the_string_to_split, '\t'); s; ++s) {
//         printf("%*s\n", s.length(), s.field());
//     }

namespace butil {

// 控制当字符串分隔迭代遇到连续分隔符时，采取的动作（默认跳过 SKIP_EMPTY_FIELD ）。
enum EmptyFieldAction {
    SKIP_EMPTY_FIELD,
    ALLOW_EMPTY_FIELD
};

// Split a string with one character
// 
// 根据一个字符分隔字符串。
class StringSplitter {
public:
    // Split `input' with `separator'. If `action' is SKIP_EMPTY_FIELD, zero-
    // length() field() will be skipped.
    // 
    // 用 |separator| 分隔 |input| 。如果 |action| 是 SKIP_EMPTY_FIELD ，则跳过
    // 空字段。
    inline StringSplitter(const char* input, char separator,
                          EmptyFieldAction action = SKIP_EMPTY_FIELD);
    inline StringSplitter(const char* str_begin, const char* str_end,
                          char separator,
                          EmptyFieldAction = SKIP_EMPTY_FIELD);

    // Move splitter forward.
    // 
    // 前向迭代
    inline StringSplitter& operator++();
    // 后缀自增
    inline StringSplitter operator++(int);

    // True iff field() is valid.
    // 
    // 解引用当前迭代字段 field()
    inline operator const void*() const;

    // Beginning address and length of the field. *(field() + length()) may
    // not be '\0' because we don't modify `input'.
    // 
    // 起始地址和字段长度。 *(field() + length()) 可能不是 '\0' 结尾，因为我们不修改 
    // |input| 。
    inline const char* field() const;
    inline size_t length() const;

    // Cast field to specific type, and write the value into `pv'.
    // Returns 0 on success, -1 otherwise.
    // NOTE: If separator is a digit, casting functions always return -1.
    // 
    // 将字段强制转换为特定类型，并将值写入 |pv| 。 成功时返回 0 ，否则返回 -1 。
    // 注意：如果 separator 是一个数字，则转换函数始终返回 -1 。
    inline int to_int8(int8_t *pv) const;
    inline int to_uint8(uint8_t *pv) const;
    inline int to_int(int *pv) const;
    inline int to_uint(unsigned int *pv) const;
    inline int to_long(long *pv) const;
    inline int to_ulong(unsigned long *pv) const;
    inline int to_longlong(long long *pv) const;
    inline int to_ulonglong(unsigned long long *pv) const;
    inline int to_float(float *pv) const;
    inline int to_double(double *pv) const;
    
private:
    inline bool not_end(const char* p) const;
    inline void init();
    
    const char* _head;  // 当前迭代字段开始地址
    const char* _tail;  // 当前迭代字段结尾地址
    const char* _str_tail;  // 源字符串结尾指针，为 c-style 时 ==NULL（无另需结尾标志）
    const char _sep;    // 分隔符
    const EmptyFieldAction _empty_field_action;
};

// Split a string with one of the separators
// 
// 根据多个字符分隔的字符串。多个分隔字符有一种类似 "或" 的关系。
// 
// Use like:
// StringMultiSplitter("jjj;xxx,kkk", ",;");
class StringMultiSplitter {
public:
    // Split `input' with one character of `separators'. If `action' is
    // SKIP_EMPTY_FIELD, zero-length() field() will be skipped.
    // NOTE: This utility stores pointer of `separators' directly rather than
    //       copying the content because this utility is intended to be used
    //       in ad-hoc manner where lifetime of `separators' is generally
    //       longer than this utility.
    inline StringMultiSplitter(const char* input, const char* separators,
                               EmptyFieldAction action = SKIP_EMPTY_FIELD);
    inline StringMultiSplitter(const char* str_begin, const char* str_end,
                               const char* separators,
                               EmptyFieldAction action = SKIP_EMPTY_FIELD);

    // Move splitter forward.
    inline StringMultiSplitter& operator++();
    inline StringMultiSplitter operator++(int);

    // True iff field() is valid.
    inline operator const void*() const;

    // Beginning address and length of the field. *(field() + length()) may
    // not be '\0' because we don't modify `input'.
    inline const char* field() const;
    inline size_t length() const;

    // Cast field to specific type, and write the value into `pv'.
    // Returns 0 on success, -1 otherwise.
    // NOTE: If separators contains digit, casting functions always return -1.
    inline int to_int8(int8_t *pv) const;
    inline int to_uint8(uint8_t *pv) const;
    inline int to_int(int *pv) const;
    inline int to_uint(unsigned int *pv) const;
    inline int to_long(long *pv) const;
    inline int to_ulong(unsigned long *pv) const;
    inline int to_longlong(long long *pv) const;
    inline int to_ulonglong(unsigned long long *pv) const;
    inline int to_float(float *pv) const;
    inline int to_double(double *pv) const;

private:
    // 字符 |c| 是否是分隔符
    inline bool is_sep(char c) const;
    inline bool not_end(const char* p) const;
    inline void init();
    
    const char* _head;
    const char* _tail;
    const char* _str_tail;
    const char* const _seps;
    const EmptyFieldAction _empty_field_action;
};

}  // namespace butil

#include "butil/string_splitter_inl.h"

#endif  // BUTIL_STRING_SPLITTER_H

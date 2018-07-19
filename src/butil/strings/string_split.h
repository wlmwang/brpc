// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_STRINGS_STRING_SPLIT_H_
#define BUTIL_STRINGS_STRING_SPLIT_H_

#include <string>
#include <utility>
#include <vector>

#include "butil/base_export.h"
#include "butil/strings/string16.h"

namespace butil {

// Splits |str| into a vector of strings delimited by |c|, placing the results
// in |r|. If several instances of |c| are contiguous, or if |str| begins with
// or ends with |c|, then an empty string is inserted.
//
// Every substring is trimmed of any leading or trailing white space.
// NOTE: |c| must be in BMP (Basic Multilingual Plane)
// 
// 转换由 |c| 字符分隔的 |str| 字符串到字符串数组，将结果放在 |r| 中。如果 |str| 中
// 有 N 个 |c| 是连续的，或者如果 |str| 以 N 个 |c| 开头或结尾，将插入 N 个空字符串。
// 每个子字符串的任何前导或尾随的空格也将被修剪。
// 
// 注意：|c| 必须在 BMP（基本平面 UTF16）。
BUTIL_EXPORT void SplitString(const string16& str,
                             char16 c,
                             std::vector<string16>* r);

// |str| should not be in a multi-byte encoding like Shift-JIS or GBK in which
// the trailing byte of a multi-byte character can be in the ASCII range.
// UTF-8, and other single/multi-byte ASCII-compatible encodings are OK.
// Note: |c| must be in the ASCII range.
// 
// 转换由 |c| 字符分隔的 |str| 字符串到字符串数组，将结果放在 |r| 中。如果 |str| 中
// 有 N 个 |c| 是连续的，或者如果 |str| 以 N 个 |c| 开头或结尾，将插入 N 个空字符串。
// 每个子字符串的任何前导或尾随的空格也将被修剪。
// 
// |str| 不应该是像 Shift-JIS 或 GBK 这样的多字节编码。 UTF-8 和其他单/多字节 ASCII 
// 兼容 (ASCII-compatible) 编码可以。注意： |c| 必须在 ASCII 范围内。
BUTIL_EXPORT void SplitString(const std::string& str,
                             char c,
                             std::vector<std::string>* r);

typedef std::vector<std::pair<std::string, std::string> > StringPairs;

// Splits |line| into key value pairs according to the given delimiters and
// removes whitespace leading each key and trailing each value. Returns true
// only if each pair has a non-empty key and value. |key_value_pairs| will
// include ("","") pairs for entries without |key_value_delimiter|.
// 
// 根据给定 key=value 分隔符 |key_value_delimiter| ，key-value 分隔符 
// |key_value_pair_delimiter| ，分隔字符串 |line| 到 |key_value_pairs| 数组中。
// 只有当每一对都有非空键和值时才返回 true 。
// 
// Use like:
// std::string strs("key1=value1;key2===value2=value");
// butil::StringPairs pairs;
// butil::SplitStringIntoKeyValuePairs(strs, '=', ';', &pairs);
// 
// ---result pairs---
// std::vector( std::pair(std::string("key1"), std::string("value1")), 
// std::pair(std::string("key2"), std::string("value2=value")) );
BUTIL_EXPORT bool SplitStringIntoKeyValuePairs(const std::string& line,
                                              char key_value_delimiter,
                                              char key_value_pair_delimiter,
                                              StringPairs* key_value_pairs);

// The same as SplitString, but use a substring delimiter instead of a char.
// 
// 与 SplitString 功能类似，区别是分隔符是一个 |s| 子字符串。
BUTIL_EXPORT void SplitStringUsingSubstr(const string16& str,
                                        const string16& s,
                                        std::vector<string16>* r);
BUTIL_EXPORT void SplitStringUsingSubstr(const std::string& str,
                                        const std::string& s,
                                        std::vector<std::string>* r);

// The same as SplitString, but don't trim white space.
// NOTE: |c| must be in BMP (Basic Multilingual Plane)
BUTIL_EXPORT void SplitStringDontTrim(const string16& str,
                                     char16 c,
                                     std::vector<string16>* r);
// |str| should not be in a multi-byte encoding like Shift-JIS or GBK in which
// the trailing byte of a multi-byte character can be in the ASCII range.
// UTF-8, and other single/multi-byte ASCII-compatible encodings are OK.
// Note: |c| must be in the ASCII range.
// 
// 与 SplitString 功能类似，区别是结果集不去除空白符。
BUTIL_EXPORT void SplitStringDontTrim(const std::string& str,
                                     char c,
                                     std::vector<std::string>* r);

// WARNING: this uses whitespace as defined by the HTML5 spec. If you need
// a function similar to this but want to trim all types of whitespace, then
// factor this out into a function that takes a string containing the characters
// that are treated as whitespace.
//
// Splits the string along whitespace (where whitespace is the five space
// characters defined by HTML 5). Each contiguous block of non-whitespace
// characters is added to result.
// 
// 警告：使用 HTML5 规范定义的空格。如果你需要一个类似于这个的函数，但是想要修剪所有类型的
// 空白，那么把它分解成一个函数，该函数接受一个包含被视为空白字符的字符串。
//
// 将字符串按空格拆分（其中空格是由 HTML5 定义的五个空格字符）。每个连续的非空白字符块被添
// 加到结果中。
BUTIL_EXPORT void SplitStringAlongWhitespace(const string16& str,
                                            std::vector<string16>* result);
BUTIL_EXPORT void SplitStringAlongWhitespace(const std::string& str,
                                            std::vector<std::string>* result);

}  // namespace butil

#endif  // BUTIL_STRINGS_STRING_SPLIT_H_

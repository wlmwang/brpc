// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 2 字节字符串类型 string16 。它能够兼容处理 UTF-16 编码的数据

#ifndef BUTIL_STRINGS_STRING16_H_
#define BUTIL_STRINGS_STRING16_H_

// WHAT:
// A version of std::basic_string that provides 2-byte characters even when
// wchar_t is not implemented as a 2-byte type. You can access this class as
// string16. We also define char16, which string16 is based upon.
//
// WHY:
// On Windows, wchar_t is 2 bytes, and it can conveniently handle UTF-16/UCS-2
// data. Plenty of existing code operates on strings encoded as UTF-16.
//
// On many other platforms, sizeof(wchar_t) is 4 bytes by default. We can make
// it 2 bytes by using the GCC flag -fshort-wchar. But then std::wstring fails
// at run time, because it calls some functions (like wcslen) that come from
// the system's native C library -- which was built with a 4-byte wchar_t!
// It's wasteful to use 4-byte wchar_t strings to carry UTF-16 data, and it's
// entirely improper on those systems where the encoding of wchar_t is defined
// as UTF-32.
//
// Here, we define string16, which is similar to std::wstring but replaces all
// libc functions with custom, 2-byte-char compatible routines. It is capable
// of carrying UTF-16-encoded data.
// 
// WHAT:
// 即使 wchar_t 没有被实现为 2 字节类型，我们也提供 2 字节版本的 std::basic_string
// 可以使用 string16 来使用它。我们也定义了 char16 类型。 string16 便是基于它。
// 
// WHY:
// 在 Windows 上，wchar_t 为 2 字节，可方便地处理 UTF-16/UCS-2 数据。现有的大量代码
// 对编码为 UTF-16 的字符串进行操作。
// 
// 在许多其他平台上，默认情况下 sizeof(wchar_t) 为 4 个字节。
// 我们可以通过使用 GCC 标志 -fshort-wchar 使其为 2 个字节。但是 std::wstring 在运行
// 时会有问题，因为它调用了一些来自本机系统 C 函数库（如 wcslen ），它是使用 4 字节的 wchar_t
// 使用 4 字节 wchar_t 字符串来携带 UTF-16 数据是浪费的，在 UTF-32 被编码为 wchar_t 的
// 那些系统上完全不正确。
// 
// 这里我们定义 string16 ，它类似于 std::wstring ，但是它是用自定义的 2 字节 char 兼容
// 的函数来替换所有的 libc 库函数。它能够兼容处理 UTF-16 编码的数据

#include <stdio.h>
#include <string>

#include "butil/base_export.h"
#include "butil/basictypes.h"

// 2 字节长度的 wchar_t
#if defined(WCHAR_T_IS_UTF16)

namespace butil {

typedef wchar_t char16;
typedef std::wstring string16;
typedef std::char_traits<wchar_t> string16_char_traits;

}  // namespace butil

// 4 字节长度的 wchar_t
#elif defined(WCHAR_T_IS_UTF32)

namespace butil {

typedef uint16_t char16;

// char16 versions of the functions required by string16_char_traits; these
// are based on the wide character functions of similar names ("w" or "wcs"
// instead of "c16").
// 
// 自定义的 2 字节 char 兼容 C 的函数
BUTIL_EXPORT int c16memcmp(const char16* s1, const char16* s2, size_t n);
BUTIL_EXPORT size_t c16len(const char16* s);
BUTIL_EXPORT const char16* c16memchr(const char16* s, char16 c, size_t n);
BUTIL_EXPORT char16* c16memmove(char16* s1, const char16* s2, size_t n);
BUTIL_EXPORT char16* c16memcpy(char16* s1, const char16* s2, size_t n);
BUTIL_EXPORT char16* c16memset(char16* s, char16 c, size_t n);

struct string16_char_traits {
  typedef char16 char_type;
  typedef int int_type;

  // int_type needs to be able to hold each possible value of char_type, and in
  // addition, the distinct value of eof().
  // 
  // int_type 需要能够保存 char_type 的每个可能的值，以及 eof() 的不同值
  COMPILE_ASSERT(sizeof(int_type) > sizeof(char_type), unexpected_type_width);

  // \file #include <ios>
  // 典型为 typedef long long streamoff;
  // 表示足以表示操作系统所支持的最大可能文件大小的有符号整数类型
  typedef std::streamoff off_type;
  // \file #include <cwchar>
  // struct mbstate_t;
  // 表示任何能出现于实现定义的受支持多字节编码规则集合的转换状态
  typedef mbstate_t state_type;
  // \file #include <ios>
  // template<class State> class fpos;
  // 类模板 std::fpos 的特化。标识流或文件中的绝对位置
  // 每个 fpos 类型对象保有流中的字节位置（典型地为 std::streamoff 类型作为私有成员）和
  // 当前迁移状态， State 类型值（典型地为 std::mbstate_t ）
  typedef std::fpos<state_type> pos_type;

  static void assign(char_type& c1, const char_type& c2) {
    c1 = c2;
  }

  static bool eq(const char_type& c1, const char_type& c2) {
    return c1 == c2;
  }
  static bool lt(const char_type& c1, const char_type& c2) {
    return c1 < c2;
  }

  static int compare(const char_type* s1, const char_type* s2, size_t n) {
    return c16memcmp(s1, s2, n);
  }

  static size_t length(const char_type* s) {
    return c16len(s);
  }

  static const char_type* find(const char_type* s, size_t n,
                               const char_type& a) {
    return c16memchr(s, a, n);
  }

  static char_type* move(char_type* s1, const char_type* s2, int_type n) {
    return c16memmove(s1, s2, n);
  }

  static char_type* copy(char_type* s1, const char_type* s2, size_t n) {
    return c16memcpy(s1, s2, n);
  }

  static char_type* assign(char_type* s, size_t n, char_type a) {
    return c16memset(s, a, n);
  }

  static int_type not_eof(const int_type& c) {
    return eq_int_type(c, eof()) ? 0 : c;
  }

  static char_type to_char_type(const int_type& c) {
    return char_type(c);
  }

  static int_type to_int_type(const char_type& c) {
    return int_type(c);
  }

  static bool eq_int_type(const int_type& c1, const int_type& c2) {
    return c1 == c2;
  }

  static int_type eof() {
    return static_cast<int_type>(EOF);
  }
};

// string16 字符串类型
typedef std::basic_string<char16, butil::string16_char_traits> string16;

// string16 << 运算符重载。将 UTF-16 转换成 UTF-8 输出
BUTIL_EXPORT extern std::ostream& operator<<(std::ostream& out,
                                            const string16& str);

// This is required by googletest to print a readable output on test failures.
BUTIL_EXPORT extern void PrintTo(const string16& str, std::ostream* out);

}  // namespace butil

// The string class will be explicitly instantiated only once, in string16.cc.
//
// std::basic_string<> in GNU libstdc++ contains a static data member,
// _S_empty_rep_storage, to represent empty strings.  When an operation such
// as assignment or destruction is performed on a string, causing its existing
// data member to be invalidated, it must not be freed if this static data
// member is being used.  Otherwise, it counts as an attempt to free static
// (and not allocated) data, which is a memory error.
//
// Generally, due to C++ template magic, _S_empty_rep_storage will be marked
// as a coalesced symbol, meaning that the linker will combine multiple
// instances into a single one when generating output.
//
// If a string class is used by multiple shared libraries, a problem occurs.
// Each library will get its own copy of _S_empty_rep_storage.  When strings
// are passed across a library boundary for alteration or destruction, memory
// errors will result.  GNU libstdc++ contains a configuration option,
// --enable-fully-dynamic-string (_GLIBCXX_FULLY_DYNAMIC_STRING), which
// disables the static data member optimization, but it's a good optimization
// and non-STL code is generally at the mercy of the system's STL
// configuration.  Fully-dynamic strings are not the default for GNU libstdc++
// libstdc++ itself or for the libstdc++ installations on the systems we care
// about, such as Mac OS X and relevant flavors of Linux.
//
// See also http://gcc.gnu.org/bugzilla/show_bug.cgi?id=24196 .
//
// To avoid problems, string classes need to be explicitly instantiated only
// once, in exactly one library.  All other string users see it via an "extern"
// declaration.  This is precisely how GNU libstdc++ handles
// std::basic_string<char> (string) and std::basic_string<wchar_t> (wstring).
//
// This also works around a Mac OS X linker bug in ld64-85.2.1 (Xcode 3.1.2),
// in which the linker does not fully coalesce symbols when dead code
// stripping is enabled.  This bug causes the memory errors described above
// to occur even when a std::basic_string<> does not cross shared library
// boundaries, such as in statically-linked executables.
//
// TODO(mark): File this bug with Apple and update this note with a bug number.

// std::basic_string 外部模版显式实例化声明

extern template
class BUTIL_EXPORT std::basic_string<butil::char16, butil::string16_char_traits>;

#endif  // WCHAR_T_IS_UTF32

#endif  // BUTIL_STRINGS_STRING16_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copied from strings/stringpiece.h with modifications
//
// A string-like object that points to a sized piece of memory.
//
// You can use StringPiece as a function or method parameter.  A StringPiece
// parameter can receive a double-quoted string literal argument, a "const
// char*" argument, a string argument, or a StringPiece argument with no data
// copying.  Systematic use of StringPiece for arguments reduces data
// copies and strlen() calls.
//
// Prefer passing StringPieces by value:
//   void MyFunction(StringPiece arg);
// If circumstances require, you may also pass by const reference:
//   void MyFunction(const StringPiece& arg);  // not preferred
// Both of these have the same lifetime semantics.  Passing by value
// generates slightly smaller code.  For more discussion, Googlers can see
// the thread go/stringpiecebyvalue on c-users.
//
// StringPiece16 is similar to StringPiece but for butil::string16 instead of
// std::string. We do not define as large of a subset of the STL functions
// from basic_string as in StringPiece, but this can be changed if these
// functions (find, find_first_of, etc.) are found to be useful in this context.
// 
// 类似 string 的对象，不同的是：
// 1. StringPieces 不负责管理(分配、释放)字符串的内存（没有所有权，也不能控制其生命周期）
// 2. StringPieces 常用在一些用到字符串作为参数的 "函数/方法" 上
// 
// 可以使用 StringPiece 作为函数或方法参数。系统地使用 StringPiece 作为参数可以减少数据
// 拷贝和 strlen() 的调用。 StringPiece 构造参数可以接收：
// 1. C-style 字符串字面量参数
// 2. "const char *"/"const char16 *" 参数
// 3. string/string16 引用参数 
// 4. StringPiece 参数（浅复制）
// 5. StringPiece 引用参数
// 
// StringPieces 最好通过值传递，在一些特殊情况下也可以通过 const 引用传递（不建议）。这
// 两个都具有相同的生命周期语义（都没有字符串所有权），但后者传递值会产生略小的代码。 
//

#ifndef BUTIL_STRINGS_STRING_PIECE_H_
#define BUTIL_STRINGS_STRING_PIECE_H_

#include <stddef.h>

#include <iosfwd>
#include <string>

#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/containers/hash_tables.h"
#include "butil/strings/string16.h"

namespace butil {

template <typename STRING_TYPE> class BasicStringPiece;
typedef BasicStringPiece<std::string> StringPiece;
typedef BasicStringPiece<string16> StringPiece16;

// internal --------------------------------------------------------------------

// Many of the StringPiece functions use different implementations for the
// 8-bit and 16-bit versions, and we don't want lots of template expansions in
// this (very common) header that will slow down compilation.
//
// So here we define overloaded functions called by the StringPiece template.
// For those that share an implementation, the two versions will expand to a
// template internal to the .cc file.
namespace internal {

// 每个函数都提供了两个类型的函数重载 StringPiece/StringPiece16

BUTIL_EXPORT void CopyToString(const StringPiece& self, std::string* target);
BUTIL_EXPORT void CopyToString(const StringPiece16& self, string16* target);

BUTIL_EXPORT void AppendToString(const StringPiece& self, std::string* target);
BUTIL_EXPORT void AppendToString(const StringPiece16& self, string16* target);

BUTIL_EXPORT size_t copy(const StringPiece& self,
                        char* buf,
                        size_t n,
                        size_t pos);
BUTIL_EXPORT size_t copy(const StringPiece16& self,
                        char16* buf,
                        size_t n,
                        size_t pos);

BUTIL_EXPORT size_t find(const StringPiece& self,
                        const StringPiece& s,
                        size_t pos);
BUTIL_EXPORT size_t find(const StringPiece16& self,
                        const StringPiece16& s,
                        size_t pos);
BUTIL_EXPORT size_t find(const StringPiece& self,
                        char c,
                        size_t pos);
BUTIL_EXPORT size_t find(const StringPiece16& self,
                        char16 c,
                        size_t pos);

BUTIL_EXPORT size_t rfind(const StringPiece& self,
                         const StringPiece& s,
                         size_t pos);
BUTIL_EXPORT size_t rfind(const StringPiece16& self,
                         const StringPiece16& s,
                         size_t pos);
BUTIL_EXPORT size_t rfind(const StringPiece& self,
                         char c,
                         size_t pos);
BUTIL_EXPORT size_t rfind(const StringPiece16& self,
                         char16 c,
                         size_t pos);

BUTIL_EXPORT size_t find_first_of(const StringPiece& self,
                                 const StringPiece& s,
                                 size_t pos);
BUTIL_EXPORT size_t find_first_of(const StringPiece16& self,
                                 const StringPiece16& s,
                                 size_t pos);

BUTIL_EXPORT size_t find_first_not_of(const StringPiece& self,
                                     const StringPiece& s,
                                     size_t pos);
BUTIL_EXPORT size_t find_first_not_of(const StringPiece16& self,
                                     const StringPiece16& s,
                                     size_t pos);
BUTIL_EXPORT size_t find_first_not_of(const StringPiece& self,
                                     char c,
                                     size_t pos);
BUTIL_EXPORT size_t find_first_not_of(const StringPiece16& self,
                                     char16 c,
                                     size_t pos);

BUTIL_EXPORT size_t find_last_of(const StringPiece& self,
                                const StringPiece& s,
                                size_t pos);
BUTIL_EXPORT size_t find_last_of(const StringPiece16& self,
                                const StringPiece16& s,
                                size_t pos);
BUTIL_EXPORT size_t find_last_of(const StringPiece& self,
                                char c,
                                size_t pos);
BUTIL_EXPORT size_t find_last_of(const StringPiece16& self,
                                char16 c,
                                size_t pos);

BUTIL_EXPORT size_t find_last_not_of(const StringPiece& self,
                                    const StringPiece& s,
                                    size_t pos);
BUTIL_EXPORT size_t find_last_not_of(const StringPiece16& self,
                                    const StringPiece16& s,
                                    size_t pos);
BUTIL_EXPORT size_t find_last_not_of(const StringPiece16& self,
                                    char16 c,
                                    size_t pos);
BUTIL_EXPORT size_t find_last_not_of(const StringPiece& self,
                                    char c,
                                    size_t pos);

BUTIL_EXPORT StringPiece substr(const StringPiece& self,
                               size_t pos,
                               size_t n);
BUTIL_EXPORT StringPiece16 substr(const StringPiece16& self,
                                 size_t pos,
                                 size_t n);

}  // namespace internal

// BasicStringPiece ------------------------------------------------------------

// Defines the types, methods, operators, and data members common to both
// StringPiece and StringPiece16. Do not refer to this class directly, but
// rather to BasicStringPiece, StringPiece, or StringPiece16.
//
// This is templatized by string class type rather than character type, so
// BasicStringPiece<std::string> or BasicStringPiece<butil::string16>.
// 
// 这是一个字符串类型而不是字符类型的模板（标准库字符模版 std::basic_string<char>）
// BasicStringPiece<std::string> 或 BasicStringPiece<butil::string16>
template <typename STRING_TYPE> class BasicStringPiece {
 public:
  // Standard STL container boilerplate.
  // 
  // 标准 STL 定义建议要求
  typedef size_t size_type;
  // std::string 对应 char ，butil::string16 对应 char16
  typedef typename STRING_TYPE::value_type value_type;
  typedef const value_type* pointer;
  typedef const value_type& reference;
  typedef const value_type& const_reference;
  typedef ptrdiff_t difference_type;
  typedef const value_type* const_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  // 非法索引值
  static const size_type npos;

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "string" wherever a "StringPiece" is
  // expected (likewise for char16, string16, StringPiece16).
  // 
  // 提供 non-explicit 的构造函数。因此用户可以在需要 "StringPiece" 的地方传入
  // "const char*"/"const char16*"/const string&/const string16& 以及其迭
  // 代器，显示构造 StringPiece
  BasicStringPiece() : ptr_(NULL), length_(0) {}
  BasicStringPiece(const value_type* str)
      : ptr_(str),
        length_((str == NULL) ? 0 : STRING_TYPE::traits_type::length(str)) {}
  BasicStringPiece(const STRING_TYPE& str)
      : ptr_(str.data()), length_(str.size()) {}
  BasicStringPiece(const value_type* offset, size_type len)
      : ptr_(offset), length_(len) {}
  BasicStringPiece(const typename STRING_TYPE::const_iterator& begin,
                    const typename STRING_TYPE::const_iterator& end)
      : ptr_((end > begin) ? &(*begin) : NULL),
        length_((end > begin) ? (size_type)(end - begin) : 0) {}

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.
  // 
  // data() 可能是 '\0' 结尾，也可能不是
  const value_type* data() const { return ptr_; }
  size_type size() const { return length_; }
  size_type length() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() {
    ptr_ = NULL;
    length_ = 0;
  }
  void set(const value_type* data, size_type len) {
    ptr_ = data;
    length_ = len;
  }
  void set(const value_type* str) {
    ptr_ = str;
    length_ = str ? STRING_TYPE::traits_type::length(str) : 0;
  }

  value_type operator[](size_type i) const { return ptr_[i]; }

  // 移除指定长度字符
  void remove_prefix(size_type n) {
    ptr_ += n;
    length_ -= n;
  }

  void remove_suffix(size_type n) {
    length_ -= n;
  }

  // Remove heading and trailing spaces.
  // 
  // 移除前导、结尾空白
  void trim_spaces() {
    size_t nsp = 0;
    for (; nsp < size() && isspace(ptr_[nsp]); ++nsp) {}
    remove_prefix(nsp);
    nsp = 0;
    for (; nsp < size() && isspace(ptr_[size()-1-nsp]); ++nsp) {}
    remove_suffix(nsp);
  }       

  // BasicStringPiece<string>/BasicStringPiece<string16> 混合比较
  int compare(const BasicStringPiece<STRING_TYPE>& x) const {
    // 先比较较短的字符
    int r = wordmemcmp(
        ptr_, x.ptr_, (length_ < x.length_ ? length_ : x.length_));
    if (r == 0) {
      if (length_ < x.length_) r = -1;
      else if (length_ > x.length_) r = +1;
    }
    return r;
  }

  // 转换原始类型
  STRING_TYPE as_string() const {
    // std::string doesn't like to take a NULL pointer even with a 0 size.
    // 
    // std::string 不喜欢采用 NULL 指针，即使是 0 的大小
    return empty() ? STRING_TYPE() : STRING_TYPE(data(), size());
  }

  // Return the first/last character, UNDEFINED when StringPiece is empty.
  // 
  // 返回首|尾字符。当 BasicStringPiece 为空，行为未定义
  char front() const { return *ptr_; }
  char back() const { return *(ptr_ + length_ - 1); }
  // Return the first/last character, 0 when StringPiece is empty.
  // 
  // 安全返回首|尾字符，。BasicStringPiece 为空时，返回 '\0'
  char front_or_0() const { return length_ ? *ptr_ : '\0'; }
  char back_or_0() const { return length_ ? *(ptr_ + length_ - 1) : '\0'; }

  // 迭代器
  const_iterator begin() const { return ptr_; }
  const_iterator end() const { return ptr_ + length_; }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(ptr_ + length_);
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(ptr_);
  }

  // BasicStringPiece 并不持有字符串的所有权，故 max_size()|capacity() 只是
  // 简单的等于自身 length()
  size_type max_size() const { return length_; }
  size_type capacity() const { return length_; }

  static int wordmemcmp(const value_type* p,
                        const value_type* p2,
                        size_type N) {
    return STRING_TYPE::traits_type::compare(p, p2, N);
  }

  // Sets the value of the given string target type to be the current string.
  // This saves a temporary over doing |a = b.as_string()|
  // 
  // 将给定的字符串目标类型的值设置为当前字符串。可以节省临时覆盖的时间 |a=b.as_string()|
  void CopyToString(STRING_TYPE* target) const {
    internal::CopyToString(*this, target);
  }

  void AppendToString(STRING_TYPE* target) const {
    internal::AppendToString(*this, target);
  }

  size_type copy(value_type* buf, size_type n, size_type pos = 0) const {
    return internal::copy(*this, buf, n, pos);
  }

  // Does "this" start with "x"
  // 
  // 字符串前缀匹配
  bool starts_with(const BasicStringPiece& x) const {
    return ((this->length_ >= x.length_) &&
            (wordmemcmp(this->ptr_, x.ptr_, x.length_) == 0));
  }

  // Does "this" end with "x"
  // 
  // 字符串结尾匹配
  bool ends_with(const BasicStringPiece& x) const {
    return ((this->length_ >= x.length_) &&
            (wordmemcmp(this->ptr_ + (this->length_-x.length_),
                        x.ptr_, x.length_) == 0));
  }

  // find: Search for a character or substring at a given offset.
  // 
  // 从指定偏移查找子字符串位置
  size_type find(const BasicStringPiece<STRING_TYPE>& s,
                 size_type pos = 0) const {
    return internal::find(*this, s, pos);
  }
  size_type find(value_type c, size_type pos = 0) const {
    return internal::find(*this, c, pos);
  }

  // rfind: Reverse find.
  // 
  // find 的逆向查找版本
  size_type rfind(const BasicStringPiece& s,
                  size_type pos = BasicStringPiece::npos) const {
    return internal::rfind(*this, s, pos);
  }
  size_type rfind(value_type c, size_type pos = BasicStringPiece::npos) const {
    return internal::rfind(*this, c, pos);
  }

  // find_first_of: Find the first occurence of one of a set of characters.
  // 
  // 查找一组字符中任意一个字符在字符串中第一个出现位置
  size_type find_first_of(const BasicStringPiece& s,
                          size_type pos = 0) const {
    return internal::find_first_of(*this, s, pos);
  }
  size_type find_first_of(value_type c, size_type pos = 0) const {
    return find(c, pos);
  }

  // find_first_not_of: Find the first occurence not of a set of characters.
  // 
  // 查找一组字符中任意一个字符不在字符串中第一个位置
  size_type find_first_not_of(const BasicStringPiece& s,
                              size_type pos = 0) const {
    return internal::find_first_not_of(*this, s, pos);
  }
  size_type find_first_not_of(value_type c, size_type pos = 0) const {
    return internal::find_first_not_of(*this, c, pos);
  }

  // find_last_of: Find the last occurence of one of a set of characters.
  // 
  // 查找一组字符中任意一个字符在字符串中最后一个出现位置
  size_type find_last_of(const BasicStringPiece& s,
                         size_type pos = BasicStringPiece::npos) const {
    return internal::find_last_of(*this, s, pos);
  }
  size_type find_last_of(value_type c,
                         size_type pos = BasicStringPiece::npos) const {
    return rfind(c, pos);
  }

  // find_last_not_of: Find the last occurence not of a set of characters.
  // 
  // 查找一组字符中任意一个字符不在字符串中最一个位置
  size_type find_last_not_of(const BasicStringPiece& s,
                             size_type pos = BasicStringPiece::npos) const {
    return internal::find_last_not_of(*this, s, pos);
  }
  size_type find_last_not_of(value_type c,
                             size_type pos = BasicStringPiece::npos) const {
    return internal::find_last_not_of(*this, c, pos);
  }

  // substr.
  // 
  // 截取子字符串（同样也不管理字符串内存生命周期）
  BasicStringPiece substr(size_type pos,
                          size_type n = BasicStringPiece::npos) const {
    return internal::substr(*this, pos, n);
  }

 protected:
  const value_type* ptr_;
  size_type     length_;
};

template <typename STRING_TYPE>
const typename BasicStringPiece<STRING_TYPE>::size_type
BasicStringPiece<STRING_TYPE>::npos =
    typename BasicStringPiece<STRING_TYPE>::size_type(-1);

// MSVC doesn't like complex extern templates and DLLs.
// 
// MSVC 不喜欢复杂的外部模板和 DLL
// 实例化模版
#if !defined(COMPILER_MSVC)
extern template class BUTIL_EXPORT BasicStringPiece<std::string>;
extern template class BUTIL_EXPORT BasicStringPiece<string16>;
#endif

// StingPiece operators --------------------------------------------------------

BUTIL_EXPORT bool operator==(const StringPiece& x, const StringPiece& y);

inline bool operator!=(const StringPiece& x, const StringPiece& y) {
  return !(x == y);
}

inline bool operator<(const StringPiece& x, const StringPiece& y) {
  const int r = StringPiece::wordmemcmp(
      x.data(), y.data(), (x.size() < y.size() ? x.size() : y.size()));
  return ((r < 0) || ((r == 0) && (x.size() < y.size())));
}

inline bool operator>(const StringPiece& x, const StringPiece& y) {
  return y < x;
}

inline bool operator<=(const StringPiece& x, const StringPiece& y) {
  return !(x > y);
}

inline bool operator>=(const StringPiece& x, const StringPiece& y) {
  return !(x < y);
}

// StringPiece16 operators -----------------------------------------------------

inline bool operator==(const StringPiece16& x, const StringPiece16& y) {
  if (x.size() != y.size())
    return false;

  return StringPiece16::wordmemcmp(x.data(), y.data(), x.size()) == 0;
}

inline bool operator!=(const StringPiece16& x, const StringPiece16& y) {
  return !(x == y);
}

inline bool operator<(const StringPiece16& x, const StringPiece16& y) {
  const int r = StringPiece16::wordmemcmp(
      x.data(), y.data(), (x.size() < y.size() ? x.size() : y.size()));
  return ((r < 0) || ((r == 0) && (x.size() < y.size())));
}

inline bool operator>(const StringPiece16& x, const StringPiece16& y) {
  return y < x;
}

inline bool operator<=(const StringPiece16& x, const StringPiece16& y) {
  return !(x > y);
}

inline bool operator>=(const StringPiece16& x, const StringPiece16& y) {
  return !(x < y);
}

BUTIL_EXPORT std::ostream& operator<<(std::ostream& o,
                                     const StringPiece& piece);

// [ Ease getting first/last character of std::string before C++11 ]
// return the first/last character, UNDEFINED when the string is empty.
inline char front_char(const std::string& s) { return s[0]; }
inline char back_char(const std::string& s) { return s[s.size() - 1]; }
// return the first/last character, 0 when the string is empty.
inline char front_char_or_0(const std::string& s) { return s.empty() ? '\0' : s[0]; }
inline char back_char_or_0(const std::string& s) { return s.empty() ? '\0' : s[s.size() - 1]; }

}  // namespace butil

// Hashing ---------------------------------------------------------------------

// We provide appropriate hash functions so StringPiece and StringPiece16 can
// be used as keys in hash sets and maps.

// This hash function is copied from butil/containers/hash_tables.h. We don't
// use the ones already defined for string and string16 directly because it
// would require the string constructors to be called, which we don't want.
// 
// 提供了适当的散列函数，所以 StringPiece/StringPiece16 可以用作散列集和映射中的键
// 
// 这个散列函数是从 butil/containers/hash_tables.h 复制的。
// 我们不直接使用已经定义的 string 和 string16 ，因为我们不想调用字符串构造函数
#define HASH_STRING_PIECE(StringPieceType, string_piece)                \
  std::size_t result = 0;                                               \
  for (StringPieceType::const_iterator i = string_piece.begin();        \
       i != string_piece.end(); ++i)                                    \
    result = (result * 131) + *i;                                       \
  return result;                                                        \

namespace BUTIL_HASH_NAMESPACE {
#if defined(COMPILER_GCC)

template<>
struct hash<butil::StringPiece> {
  std::size_t operator()(const butil::StringPiece& sp) const {
    HASH_STRING_PIECE(butil::StringPiece, sp);
  }
};
template<>
struct hash<butil::StringPiece16> {
  std::size_t operator()(const butil::StringPiece16& sp16) const {
    HASH_STRING_PIECE(butil::StringPiece16, sp16);
  }
};

#elif defined(COMPILER_MSVC)

inline size_t hash_value(const butil::StringPiece& sp) {
  HASH_STRING_PIECE(butil::StringPiece, sp);
}
inline size_t hash_value(const butil::StringPiece16& sp16) {
  HASH_STRING_PIECE(butil::StringPiece16, sp16);
}

#endif  // COMPILER

}  // namespace BUTIL_HASH_NAMESPACE

#endif  // BUTIL_STRINGS_STRING_PIECE_H_

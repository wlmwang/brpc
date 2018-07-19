// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_STRINGS_STRING_TOKENIZER_H_
#define BUTIL_STRINGS_STRING_TOKENIZER_H_

#include <algorithm>
#include <string>

#include "butil/strings/string_piece.h"

namespace butil {

// StringTokenizerT is a simple string tokenizer class.  It works like an
// iterator that with each step (see the Advance method) updates members that
// refer to the next token in the input string.  The user may optionally
// configure the tokenizer to return delimiters.
//
// Warning: be careful not to pass a C string into the 2-arg constructor:
// StringTokenizer t("this is a test", " ");  // WRONG
// This will create a temporary std::string, save the begin() and end()
// iterators, and then the string will be freed before we actually start
// tokenizing it.
// Instead, use a std::string or use the 3 arg constructor of CStringTokenizer.
//
//
// EXAMPLE 1:
//
//   char input[] = "this is a test";
//   CStringTokenizer t(input, input + strlen(input), " ");
//   while (t.GetNext()) {
//     printf("%s\n", t.token().c_str());
//   }
//
// Output:
//
//   this
//   is
//   a
//   test
//
//
// EXAMPLE 2:
//
//   std::string input = "no-cache=\"foo, bar\", private";
//   StringTokenizer t(input, ", ");
//   t.set_quote_chars("\"");
//   while (t.GetNext()) {
//     printf("%s\n", t.token().c_str());
//   }
//
// Output:
//
//   no-cache="foo, bar"
//   private
//
//
// EXAMPLE 3:
//
//   bool next_is_option = false, next_is_value = false;
//   std::string input = "text/html; charset=UTF-8; foo=bar";
//   StringTokenizer t(input, "; =");
//   t.set_options(StringTokenizer::RETURN_DELIMS);
//   while (t.GetNext()) {
//     if (t.token_is_delim()) {
//       switch (*t.token_begin()) {
//         case ';':
//           next_is_option = true;
//           break;
//         case '=':
//           next_is_value = true;
//           break;
//       }
//     } else {
//       const char* label;
//       if (next_is_option) {
//         label = "option-name";
//         next_is_option = false;
//       } else if (next_is_value) {
//         label = "option-value";
//         next_is_value = false;
//       } else {
//         label = "mime-type";
//       }
//       printf("%s: %s\n", label, t.token().c_str());
//     }
//   }
//
//
// StringTokenizerT 是一个简单的字符串标记器类（部分功能上有些类似 "butil/strings/
// string_split.h"）。它像迭代器一样工作，每一步（参见 Advance 方法）都会更新输入字
// 符串中下一个标记的字符串。用户可以选择配置标记器以返回分隔符。
//
// 注：不要将 C 字符串传递到 2-arg 构造函数中：StringTokenizer t("this is a test",
// " ");  // WRONG  这将创建一个临时的 std::string ，保存 begin() 和 end() 迭代器，
// 然后在我们实际开始标记它之前，该字符串已被释放。
// 
// 相反，使用 std::string 或使用 CStringTokenizer 的 3-arg 构造函数。
// 
// EXAMPLE [1 2 3] 如上
// 

template <class str, class const_iterator>
class StringTokenizerT {
 public:
  typedef typename str::value_type char_type;

  // Options that may be pass to set_options()
  enum {
    // Specifies the delimiters should be returned as tokens
    // 
    // 指定分隔符应作为标记返回
    RETURN_DELIMS = 1 << 0,
  };

  // The string object must live longer than the tokenizer.  (In particular this
  // should not be constructed with a temporary.)
  // 
  // 字符串对象 |string|(成员 start_pos_ end_ 都只是迭代器) 必须比标记器 |delims|(成
  // 员 delims_ 是副本) 作用域更长。
  StringTokenizerT(const str& string,
                   const str& delims) {
    Init(string.begin(), string.end(), delims);
  }

  StringTokenizerT(const_iterator string_begin,
                   const_iterator string_end,
                   const str& delims) {
    Init(string_begin, string_end, delims);
  }

  // Set the options for this tokenizer.  By default, this is 0.
  // 
  // 设置标记生成器的配置。默认情况下，该值为 0
  void set_options(int options) { options_ = options; }

  // Set the characters to regard as quotes.  By default, this is empty.  When
  // a quote char is encountered, the tokenizer will switch into a mode where
  // it ignores delimiters that it finds.  It switches out of this mode once it
  // finds another instance of the quote char.  If a backslash is encountered
  // within a quoted string, then the next character is skipped.
  // 
  // 设置引号字符串。默认情况下为空。遇到引号字符串时， tokenizer 将切换到忽略它找到的分
  // 隔符的模式。一旦找到引号字符串的另一个实例，它就会退出此模式。如果在带引号的字符串中
  // 遇到反斜杠，则跳过下一个字符。
  void set_quote_chars(const str& quotes) { quotes_ = quotes; }

  // Call this method to advance the tokenizer to the next delimiter.  This
  // returns false if the tokenizer is complete.  This method must be called
  // before calling any of the token* methods.
  // 
  // 调用此方法可将标记生成器前进到下一个分隔符。如果 tokenizer 完成，则返回 false 。
  // 必须在调用任何 token* 方法之前调用此方法。
  bool GetNext() {
    if (quotes_.empty() && options_ == 0)
      return QuickGetNext();  // 没有引号模式
    else
      return FullGetNext();
  }

  // Start iterating through tokens from the beginning of the string.
  // 
  // 从字符串的开头开始迭代标记。
  void Reset() {
    token_end_ = start_pos_;
  }

  // Returns true if token is a delimiter.  When the tokenizer is constructed
  // with the RETURN_DELIMS option, this method can be used to check if the
  // returned token is actually a delimiter.
  // 
  // 如果 token 是分隔符，则返回 true 。使用 RETURN_DELIMS 选项构造标记生成器时，
  // 此方法可用于检查返回的标记是否实际上是分隔符。
  bool token_is_delim() const { return token_is_delim_; }

  // If GetNext() returned true, then these methods may be used to read the
  // value of the token.
  // 
  // 如果 GetNext() 返回 true ，则可以使用这些方法来读取标记的值。
  const_iterator token_begin() const { return token_begin_; }
  const_iterator token_end() const { return token_end_; }
  // 返回当前迭代的 token 字符串副本
  str token() const { return str(token_begin_, token_end_); }
  // 返回当前迭代的 token 的 StringPiece 对象
  butil::StringPiece token_piece() const {
    return butil::StringPiece(&*token_begin_,
                             std::distance(token_begin_, token_end_));
  }

 private:
  void Init(const_iterator string_begin,
            const_iterator string_end,
            const str& delims) {
    start_pos_ = string_begin;
    token_begin_ = string_begin;
    token_end_ = string_begin;
    end_ = string_end;
    delims_ = delims;
    options_ = 0;
    token_is_delim_ = false;
  }

  // Implementation of GetNext() for when we have no quote characters. We have
  // two separate implementations because AdvanceOne() is a hot spot in large
  // text files with large tokens.
  // 
  // 没有引号字符时， GetNext() 的底层实现。因为  AdvanceOne() 是一个具有很多 tokens 
  // 的大型文本热点，只有在处理有引号时去执行。
  bool QuickGetNext() {
    token_is_delim_ = false;
    // 跳过整个分隔符字符串
    for (;;) {
      token_begin_ = token_end_;
      if (token_end_ == end_)
        return false;
      ++token_end_;
      // 当前字符是否是分隔符的一部分（跳过整个分隔符字符串）。
      if (delims_.find(*token_begin_) == str::npos)
        break;
      // else skip over delimiter.
    }
    // 查找下一个分隔符位置，设置 token_end 标记。
    while (token_end_ != end_ && delims_.find(*token_end_) == str::npos)
      ++token_end_;
    return true;
  }

  // Implementation of GetNext() for when we have to take quotes into account.
  bool FullGetNext() {
    AdvanceState state;
    token_is_delim_ = false;
    for (;;) {
      token_begin_ = token_end_;
      if (token_end_ == end_)
        return false;
      ++token_end_;
      if (AdvanceOne(&state, *token_begin_))
        break;
      if (options_ & RETURN_DELIMS) {
        token_is_delim_ = true;
        return true;
      }
      // else skip over delimiter.
    }
    while (token_end_ != end_ && AdvanceOne(&state, *token_end_))
      ++token_end_;
    return true;
  }

  // 是否是分隔符
  bool IsDelim(char_type c) const {
    return delims_.find(c) != str::npos;
  }

  // 是否是引号
  bool IsQuote(char_type c) const {
    return quotes_.find(c) != str::npos;
  }

  struct AdvanceState {
    bool in_quote;
    bool in_escape;
    char_type quote_char;
    AdvanceState() : in_quote(false), in_escape(false), quote_char('\0') {}
  };

  // Returns true if a delimiter was not hit.
  // 
  // 如果不是分隔符，则返回 true
  bool AdvanceOne(AdvanceState* state, char_type c) {
    if (state->in_quote) {  // 引号模式
      if (state->in_escape) {
        state->in_escape = false;
      } else if (c == '\\') {
        state->in_escape = true;
      } else if (c == state->quote_char) {
        state->in_quote = false;
      }
    } else {
      if (IsDelim(c))
        return false;
      state->in_quote = IsQuote(state->quote_char = c);
    }
    return true;
  }

  const_iterator start_pos_;
  const_iterator token_begin_;
  const_iterator token_end_;
  const_iterator end_;
  str delims_;
  str quotes_;
  int options_;
  bool token_is_delim_;
};

typedef StringTokenizerT<std::string, std::string::const_iterator>
    StringTokenizer;
typedef StringTokenizerT<std::wstring, std::wstring::const_iterator>
    WStringTokenizer;
typedef StringTokenizerT<std::string, const char*> CStringTokenizer;

}  // namespace butil

#endif  // BUTIL_STRINGS_STRING_TOKENIZER_H_

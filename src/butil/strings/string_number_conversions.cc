// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/strings/string_number_conversions.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <wctype.h>

#include <limits>

#include "butil/logging.h"
#include "butil/scoped_clear_errno.h"
#include "butil/strings/utf_string_conversions.h"
#include "butil/third_party/dmg_fp/dmg_fp.h"

namespace butil {

namespace {

// |STR| 为目标类型。即， std::string/string16 字符串类型。|INT| 为源数据
// 类型。 |UINT| 为源数据类型 |INT| 无损情况下转换为无符号时对应的类型。
// |NEG| = true 时为有符号，否则为无符号。
template <typename STR, typename INT, typename UINT, bool NEG>
struct IntToStringT {
  // This is to avoid a compiler warning about unary minus on unsigned type.
  // For example, say you had the following code:
  //   template <typename INT>
  //   INT abs(INT value) { return value < 0 ? -value : value; }
  // Even though if INT is unsigned, it's impossible for value < 0, so the
  // unary minus will never be taken, the compiler will still generate a
  // warning.  We do a little specialization dance...
  // 
  // 这是为了避免编译器有关无符号类型的一元减号的警告。例如，假设您有以下代码：
  // 
  //   template <typename INT>
  //   INT abs(INT value) { return value < 0 ? -value : value; }
  // 
  // 当 INT 是无符号，value < 0 也是不可能的，因此永远不会采用一元减号，但编译器
  // 还是会生成警告。
  template <typename INT2, typename UINT2, bool NEG2>
  struct ToUnsignedT {};

  template <typename INT2, typename UINT2>
  struct ToUnsignedT<INT2, UINT2, false> {
    static UINT2 ToUnsigned(INT2 value) {
      return static_cast<UINT2>(value);
    }
  };

  template <typename INT2, typename UINT2>
  struct ToUnsignedT<INT2, UINT2, true> {
    static UINT2 ToUnsigned(INT2 value) {
      return static_cast<UINT2>(value < 0 ? -value : value);
    }
  };

  // This set of templates is very similar to the above templates, but
  // for testing whether an integer is negative.
  // 
  // 测试源数据是否为负值。模板参数 NEG 可能传递说明为有符号 (true) ，需要进
  // 一步判断实际 value 值是否为负值。
  template <typename INT2, bool NEG2>
  struct TestNegT {};
  template <typename INT2>
  struct TestNegT<INT2, false> {
    static bool TestNeg(INT2 value) {
      // value is unsigned, and can never be negative.
      // 
      // 无符号永远不能为负，与 0 比较无意义。
      return false;
    }
  };
  template <typename INT2>
  struct TestNegT<INT2, true> {
    static bool TestNeg(INT2 value) {
      // 判断源数据 value 值是否为负值。
      return value < 0;
    }
  };

  static STR IntToString(INT value) {
    // log10(2) ~= 0.3 bytes needed per bit or per byte log10(2**8) ~= 2.4.
    // So round up to allocate 3 output characters per byte, plus 1 for '-'.
    // 
    // 每字节十进制整型数需要 log10(2**8) ~= 2.4 字节。所以向上舍入为每个字节分配 
    // 3 个输出字符，加上 1 代表 '-' 。
    const int kOutputBufSize = 3 * sizeof(INT) + 1;

    // Allocate the whole string right away, we will right back to front, and
    // then return the substr of what we ended up using.
    // 
    // 立即分配整个字符串，最终返回我们使用的内容。
    STR outbuf(kOutputBufSize, 0);

    // 源数字是否为负值
    bool is_neg = TestNegT<INT, NEG>::TestNeg(value);
    // Even though is_neg will never be true when INT is parameterized as
    // unsigned, even the presence of the unary operation causes a warning.
    // 
    // 将 value 转换成无符号类型数字。
    UINT res = ToUnsignedT<INT, UINT, NEG>::ToUnsigned(value);

    // 从 value 最地位开始取出数字插入 outbuf 最右侧。
    typename STR::iterator it(outbuf.end());
    do {
      --it;
      DCHECK(it != outbuf.begin());
      // 取出 value 最低位数字，并转换为对应的字符型数字
      *it = static_cast<typename STR::value_type>((res % 10) + '0');
      res /= 10;
    } while (res != 0);
    // 添加负号
    if (is_neg) {
      --it;
      DCHECK(it != outbuf.begin());
      *it = static_cast<typename STR::value_type>('-');
    }
    return STR(it, outbuf.end());
  }
};

// Utility to convert a character to a digit in a given base
// 
// 用于将给定基数字符转换为的数字的实用程序。
template<typename CHAR, int BASE, bool BASE_LTE_10> class BaseCharToDigit {
};

// Faster specialization for bases <= 10
// 
// 基数 <= 10 的速度更快的特化版本。
template<typename CHAR, int BASE> class BaseCharToDigit<CHAR, BASE, true> {
 public:
  static bool Convert(CHAR c, uint8_t* digit) {
    // 基数 <= 10 字符转换成整型。
    if (c >= '0' && c < '0' + BASE) {
      *digit = c - '0';
      return true;
    }
    return false;
  }
};

// Specialization for bases where 10 < base <= 36
// 
// 10 < 基数 <= 36 的特化版本。
template<typename CHAR, int BASE> class BaseCharToDigit<CHAR, BASE, false> {
 public:
  static bool Convert(CHAR c, uint8_t* digit) {
    // 10 < 基数 <= 36 字符转换成整型。
    if (c >= '0' && c <= '9') {
      *digit = c - '0';
    } else if (c >= 'a' && c < 'a' + BASE - 10) {
      *digit = c - 'a' + 10;
    } else if (c >= 'A' && c < 'A' + BASE - 10) {
      *digit = c - 'A' + 10;
    } else {
      return false;
    }
    return true;
  }
};

// BaseCharToDigit<> 模版类的辅助函数（可自动类型推导）。
template<int BASE, typename CHAR> bool CharToDigit(CHAR c, uint8_t* digit) {
  return BaseCharToDigit<CHAR, BASE, BASE <= 10>::Convert(c, digit);
}

// There is an IsWhitespace for wchars defined in string_util.h, but it is
// locale independent, whereas the functions we are replacing were
// locale-dependent. TBD what is desired, but for the moment let's not introduce
// a change in behaviour.
// 
// 在 string_util.h 中定义了一个用于 wchars 的 IsWhitespace ，但它与 locale 无关，
// 而我们要替换的函数是与语言环境相关的。
// 
// 判定字符是否为空白字符的模版类。（可用于 char,char16）
template<typename CHAR> class WhitespaceHelper {
};

template<> class WhitespaceHelper<char> {
 public:
  static bool Invoke(char c) {
    return 0 != isspace(static_cast<unsigned char>(c));
  }
};

template<> class WhitespaceHelper<char16> {
 public:
  static bool Invoke(char16 c) {
    return 0 != iswspace(c);
  }
};

// WhitespaceHelper<> 模版类的辅助函数（可自动类型推导）。
template<typename CHAR> bool LocalIsWhitespace(CHAR c) {
  return WhitespaceHelper<CHAR>::Invoke(c);
}

// IteratorRangeToNumberTraits should provide:
//  - a typedef for iterator_type, the iterator type used as input.
//  - a typedef for value_type, the target numeric type.
//  - static functions min, max (returning the minimum and maximum permitted
//    values)
//  - constant kBase, the base in which to interpret the input
//  
// IteratorRangeToNumberTraits 应该提供：
// - iterator_type 的 typedef ，用作输入的迭代器类型。
// - value_type 的 typedef ，目标数字类型。
// - 静态函数 min,max （返回最小和最大允许值）。
// - 常数 kBase，解释输入的基础。
template<typename IteratorRangeToNumberTraits>
class IteratorRangeToNumber {
 public:
  typedef IteratorRangeToNumberTraits traits;
  typedef typename traits::iterator_type const_iterator;
  typedef typename traits::value_type value_type;

  // Generalized iterator-range-to-number conversion.
  //
  // 广义迭代器范围到数字转换。
  static bool Invoke(const_iterator begin,
                     const_iterator end,
                     value_type* output) {
    bool valid = true;  // 转换是否有效

    // 跳过所有前导空白符。并设置无效标志
    while (begin != end && LocalIsWhitespace(*begin)) {
      valid = false;
      ++begin;
    }

    if (begin != end && *begin == '-') {
      if (!std::numeric_limits<value_type>::is_signed) {
        valid = false;
      } else if (!Negative::Invoke(begin + 1, end, output)) {
        valid = false;
      }
    } else {
      if (begin != end && *begin == '+') {
        ++begin;
      }
      if (!Positive::Invoke(begin, end, output)) {
        valid = false;
      }
    }

    return valid;
  }

 private:
  // Sign provides:
  //  - a static function, CheckBounds, that determines whether the next digit
  //    causes an overflow/underflow
  //  - a static function, Increment, that appends the next digit appropriately
  //    according to the sign of the number being parsed.
  template<typename Sign>
  class Base {
   public:
    static bool Invoke(const_iterator begin, const_iterator end,
                       typename traits::value_type* output) {
      *output = 0;

      if (begin == end) {
        return false;
      }

      // Note: no performance difference was found when using template
      // specialization to remove this check in bases other than 16
      // 
      // 注意：跳过 16 进制字符串开头的 0X
      if (traits::kBase == 16 && end - begin > 2 && *begin == '0' &&
          (*(begin + 1) == 'x' || *(begin + 1) == 'X')) {
        begin += 2;
      }

      for (const_iterator current = begin; current != end; ++current) {
        uint8_t new_digit = 0;

        // 转换单字符为数字，写入到 new_digit 中。
        if (!CharToDigit<traits::kBase>(*current, &new_digit)) {
          return false;
        }

        if (current != begin) {
          // 是超过目标整型值域。
          if (!Sign::CheckBounds(output, new_digit)) {
            return false;
          }
          *output *= traits::kBase;
        }

        Sign::Increment(new_digit, output);
      }
      return true;
    }
  };

  // 正数转换
  class Positive : public Base<Positive> {
   public:
    // 判断 |output| + |new_digit| 是否超过目标整型的最大值。
    static bool CheckBounds(value_type* output, uint8_t new_digit) {
      if (*output > static_cast<value_type>(traits::max() / traits::kBase) ||
          (*output == static_cast<value_type>(traits::max() / traits::kBase) &&
           new_digit > traits::max() % traits::kBase)) {
        *output = traits::max();
        return false;
      }
      return true;
    }
    static void Increment(uint8_t increment, value_type* output) {
      *output += increment;
    }
  };

  // 负数转换
  class Negative : public Base<Negative> {
   public:
    // 判断 |output| - |new_digit| 是否超过目标整型的最小值。
    static bool CheckBounds(value_type* output, uint8_t new_digit) {
      if (*output < traits::min() / traits::kBase ||
          (*output == traits::min() / traits::kBase &&
           new_digit > 0 - traits::min() % traits::kBase)) {
        *output = traits::min();
        return false;
      }
      return true;
    }
    static void Increment(uint8_t increment, value_type* output) {
      *output -= increment;
    }
  };
};

// |ITERATOR| 为源字符串迭代器类型（StringPiece::const_iterator），
// |VALUE| 目标类型， |BASE| 为源字符串（迭代器）类型基数。
template<typename ITERATOR, typename VALUE, int BASE>
class BaseIteratorRangeToNumberTraits {
 public:
  typedef ITERATOR iterator_type;
  typedef VALUE value_type;
  static value_type min() {
    return std::numeric_limits<value_type>::min();
  }
  static value_type max() {
    return std::numeric_limits<value_type>::max();
  }
  static const int kBase = BASE;
};

// 16 进制字符串转换成 *int* 类型特化
template<typename ITERATOR>
class BaseHexIteratorRangeToIntTraits
    : public BaseIteratorRangeToNumberTraits<ITERATOR, int, 16> {
};

template<typename ITERATOR>
class BaseHexIteratorRangeToUIntTraits
    : public BaseIteratorRangeToNumberTraits<ITERATOR, uint32_t, 16> {
};

template<typename ITERATOR>
class BaseHexIteratorRangeToInt64Traits
    : public BaseIteratorRangeToNumberTraits<ITERATOR, int64_t, 16> {
};

template<typename ITERATOR>
class BaseHexIteratorRangeToUInt64Traits
    : public BaseIteratorRangeToNumberTraits<ITERATOR, uint64_t, 16> {
};

// 特化字符串迭代器为 StringPiece::const_iterator
typedef BaseHexIteratorRangeToIntTraits<StringPiece::const_iterator>
    HexIteratorRangeToIntTraits;

typedef BaseHexIteratorRangeToUIntTraits<StringPiece::const_iterator>
    HexIteratorRangeToUIntTraits;

typedef BaseHexIteratorRangeToInt64Traits<StringPiece::const_iterator>
    HexIteratorRangeToInt64Traits;

typedef BaseHexIteratorRangeToUInt64Traits<StringPiece::const_iterator>
    HexIteratorRangeToUInt64Traits;

// 16 进制转换成数字类型为 uint8_t 类型字节数组。如 |8A| -> |18|
// |*output| 将包含在错误之前成功解析的字节数，前导 0x 或 +/- 是不允许的。
template<typename STR>
bool HexStringToBytesT(const STR& input, std::vector<uint8_t>* output) {
  DCHECK_EQ(output->size(), 0u);
  size_t count = input.size();
  // 必须是整字节字符
  if (count == 0 || (count % 2) != 0)
    return false;
  for (uintptr_t i = 0; i < count / 2; ++i) {
    uint8_t msb = 0;  // most significant 4 bits
    uint8_t lsb = 0;  // least significant 4 bits
    if (!CharToDigit<16>(input[i * 2], &msb) ||
        !CharToDigit<16>(input[i * 2 + 1], &lsb))
      return false;
    output->push_back((msb << 4) | lsb);
  }
  return true;
}

template <typename VALUE, int BASE>
class StringPieceToNumberTraits
    : public BaseIteratorRangeToNumberTraits<StringPiece::const_iterator,
                                             VALUE,
                                             BASE> {
};

// IteratorRangeToNumber 与 StringPieceToNumberTraits 的包装器辅助函数。
// 
// 基数为 10 的 StringPiece 字符串转换为 |VALUE| 整型类型数字。
template <typename VALUE>
bool StringToIntImpl(const StringPiece& input, VALUE* output) {
  return IteratorRangeToNumber<StringPieceToNumberTraits<VALUE, 10> >::Invoke(
      input.begin(), input.end(), output);
}

template <typename VALUE, int BASE>
class StringPiece16ToNumberTraits
    : public BaseIteratorRangeToNumberTraits<StringPiece16::const_iterator,
                                             VALUE,
                                             BASE> {
};

// IteratorRangeToNumber 与 StringPiece16ToNumberTraits 的包装器辅助函数。
// 
// 基数为 10 的 StringPiece16 字符串转换为 |VALUE| 整型类型数字。
template <typename VALUE>
bool String16ToIntImpl(const StringPiece16& input, VALUE* output) {
  return IteratorRangeToNumber<StringPiece16ToNumberTraits<VALUE, 10> >::Invoke(
      input.begin(), input.end(), output);
}

}  // namespace

std::string IntToString(int value) {
  return IntToStringT<std::string, int, unsigned int, true>::
      IntToString(value);
}

string16 IntToString16(int value) {
  return IntToStringT<string16, int, unsigned int, true>::
      IntToString(value);
}

std::string UintToString(unsigned int value) {
  return IntToStringT<std::string, unsigned int, unsigned int, false>::
      IntToString(value);
}

string16 UintToString16(unsigned int value) {
  return IntToStringT<string16, unsigned int, unsigned int, false>::
      IntToString(value);
}

std::string Int64ToString(int64_t value) {
  return IntToStringT<std::string, int64_t, uint64_t, true>::IntToString(value);
}

string16 Int64ToString16(int64_t value) {
  return IntToStringT<string16, int64_t, uint64_t, true>::IntToString(value);
}

std::string Uint64ToString(uint64_t value) {
  return IntToStringT<std::string, uint64_t, uint64_t, false>::IntToString(value);
}

string16 Uint64ToString16(uint64_t value) {
  return IntToStringT<string16, uint64_t, uint64_t, false>::IntToString(value);
}

std::string SizeTToString(size_t value) {
  return IntToStringT<std::string, size_t, size_t, false>::IntToString(value);
}

string16 SizeTToString16(size_t value) {
  return IntToStringT<string16, size_t, size_t, false>::IntToString(value);
}

std::string DoubleToString(double value) {
  // According to g_fmt.cc, it is sufficient to declare a buffer of size 32.
  // 
  // 根据 g_fmt.cc ，声明一个大小为 32 的缓冲区就足够了。
  char buffer[32];
  dmg_fp::g_fmt(buffer, value);
  return std::string(buffer);
}

bool StringToInt(const StringPiece& input, int* output) {
  return StringToIntImpl(input, output);
}

bool StringToInt(const StringPiece16& input, int* output) {
  return String16ToIntImpl(input, output);
}

bool StringToUint(const StringPiece& input, unsigned* output) {
  return StringToIntImpl(input, output);
}

bool StringToUint(const StringPiece16& input, unsigned* output) {
  return String16ToIntImpl(input, output);
}

bool StringToInt64(const StringPiece& input, int64_t* output) {
  return StringToIntImpl(input, output);
}

bool StringToInt64(const StringPiece16& input, int64_t* output) {
  return String16ToIntImpl(input, output);
}

bool StringToUint64(const StringPiece& input, uint64_t* output) {
  return StringToIntImpl(input, output);
}

bool StringToUint64(const StringPiece16& input, uint64_t* output) {
  return String16ToIntImpl(input, output);
}

bool StringToSizeT(const StringPiece& input, size_t* output) {
  return StringToIntImpl(input, output);
}

bool StringToSizeT(const StringPiece16& input, size_t* output) {
  return String16ToIntImpl(input, output);
}

bool StringToDouble(const std::string& input, double* output) {
  // Thread-safe?  It is on at least Mac, Linux, and Windows.
  ScopedClearErrno clear_errno;

  char* endptr = NULL;
  *output = dmg_fp::strtod(input.c_str(), &endptr);

  // Cases to return false:
  //  - If errno is ERANGE, there was an overflow or underflow.
  //  - If the input string is empty, there was nothing to parse.
  //  - If endptr does not point to the end of the string, there are either
  //    characters remaining in the string after a parsed number, or the string
  //    does not begin with a parseable number.  endptr is compared to the
  //    expected end given the string's stated length to correctly catch cases
  //    where the string contains embedded NUL characters.
  //  - If the first character is a space, there was leading whitespace
  return errno == 0 &&
         !input.empty() &&
         input.c_str() + input.length() == endptr &&
         !isspace(input[0]);
}

// Note: if you need to add String16ToDouble, first ask yourself if it's
// really necessary. If it is, probably the best implementation here is to
// convert to 8-bit and then use the 8-bit version.

// Note: if you need to add an iterator range version of StringToDouble, first
// ask yourself if it's really necessary. If it is, probably the best
// implementation here is to instantiate a string and use the string version.

std::string HexEncode(const void* bytes, size_t size) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters.
  // 
  // 每个输入字节创建两个十六进制字符。
  std::string ret(size * 2, '\0');

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i];
    ret[(i * 2)] = kHexChars[(b >> 4) & 0xf];
    ret[(i * 2) + 1] = kHexChars[b & 0xf];
  }
  return ret;
}

bool HexStringToInt(const StringPiece& input, int* output) {
  return IteratorRangeToNumber<HexIteratorRangeToIntTraits>::Invoke(
    input.begin(), input.end(), output);
}

bool HexStringToUInt(const StringPiece& input, uint32_t* output) {
  return IteratorRangeToNumber<HexIteratorRangeToUIntTraits>::Invoke(
      input.begin(), input.end(), output);
}

bool HexStringToInt64(const StringPiece& input, int64_t* output) {
  return IteratorRangeToNumber<HexIteratorRangeToInt64Traits>::Invoke(
    input.begin(), input.end(), output);
}

bool HexStringToUInt64(const StringPiece& input, uint64_t* output) {
  return IteratorRangeToNumber<HexIteratorRangeToUInt64Traits>::Invoke(
      input.begin(), input.end(), output);
}

bool HexStringToBytes(const std::string& input, std::vector<uint8_t>* output) {
  return HexStringToBytesT(input, output);
}

}  // namespace butil

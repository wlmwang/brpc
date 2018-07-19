// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/rand_util.h"

#include <math.h>

#include <algorithm>
#include <limits>

#include "butil/basictypes.h"
#include "butil/logging.h"
#include "butil/strings/string_util.h"

namespace butil {

// 返回 [min, max] 之间 int 随机数。
int RandInt(int min, int max) {
  DCHECK_LE(min, max);

  uint64_t range = static_cast<uint64_t>(max) - min + 1;
  int result = min + static_cast<int>(butil::RandGenerator(range));
  DCHECK_GE(result, min);
  DCHECK_LE(result, max);
  return result;
}

double RandDouble() {
  return BitsToOpenEndedUnitInterval(butil::RandUint64());
}

double BitsToOpenEndedUnitInterval(uint64_t bits) {  
  // We try to get maximum precision by masking out as many bits as will fit
  // in the target type's mantissa, and raising it to an appropriate power to
  // produce output in the range [0, 1).  For IEEE 754 doubles, the mantissa
  // is expected to accommodate 53 bits.
  // 
  // 我们尝试通过屏蔽掉目标类型尾数的多个位来获得最大精度，并将其提高到适当的倍数以产
  // 生 [0,1] 范围内的输出。对于 IEEE 754 双精度，尾数预计可容纳 53 位。


  // @tips
  // ::radix 的值用于表示系统的一个数字类型的底。对于所有二进制数值类型为 2 。它可以
  // 是别的值，譬如对 IEEE 754 十进制浮点类型或第三方二进制编码十进制整数为 10 。
  // 
  // ::digits 的值是能无更改地表示类型 T 的 radix 底位数。对于整数类型，这是不含符号
  // 位和填充位（若存在）的位数。对于浮点类型，这是尾数的位数。
  
  COMPILE_ASSERT(std::numeric_limits<double>::radix == 2, otherwise_use_scalbn);
  static const int kBits = std::numeric_limits<double>::digits;
  // 保留此次随机数的尾数位数的二进制值。
  uint64_t random_bits = bits & ((GG_UINT64_C(1) << kBits) - 1);
  // @tips
  // \file <math.h>
  // double ldexp(double x, int exp);
  // 用来求一个数乘上 2 的 exp 次方的值。返回值为 x * 2^exp
  double result = ldexp(static_cast<double>(random_bits), -1 * kBits);
  DCHECK_GE(result, 0.0);
  DCHECK_LT(result, 1.0);
  return result;
}

// 返回 [0, range) 之间随机数。线程安全。
uint64_t RandGenerator(uint64_t range) {
  DCHECK_GT(range, 0u);
  // We must discard random results above this number, as they would
  // make the random generator non-uniform (consider e.g. if
  // MAX_UINT64 was 7 and |range| was 5, then a result of 1 would be twice
  // as likely as a result of 3 or 4).
  // 
  // 我们必须丢弃高于 max_acceptable_value 此数字的随机结果，因为它们会使随机生成
  // 器不均匀（例如若 MAX_UINT64 为 7 且 |range| 为 5，则结果为 1 将是 3 或 4 的
  // 结果的两倍））
  uint64_t max_acceptable_value =
      (std::numeric_limits<uint64_t>::max() / range) * range - 1;

  uint64_t value;
  do {
    value = butil::RandUint64();
  } while (value > max_acceptable_value);

  // [0, range)
  return value % range;
}

std::string RandBytesAsString(size_t length) {
  DCHECK_GT(length, 0u);
  std::string result;
  RandBytes(WriteInto(&result, length + 1), length);
  return result;
}

}  // namespace butil

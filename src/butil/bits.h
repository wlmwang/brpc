// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some bit utilities.
// 
// 一些位运算工具函数（如：算出可表示最大值为 n 最少需要多少位二进制数）

#ifndef BUTIL_BITS_H_
#define BUTIL_BITS_H_

#include "butil/basictypes.h"
#include "butil/logging.h"

namespace butil {
namespace bits {

// Returns the integer i such as 2^i <= n < 2^(i+1)
// 
// 返回 2^i（幂次方）不大于 n 的最大整数值的幂。即，不等式： 2^i <= n < 2^(i+1)
// eg. : n=6; i=2
inline int Log2Floor(uint32_t n) {
  if (n == 0)
    return -1;
  int log = 0;
  uint32_t value = n;
  // 算法核心：算出 n 的二进制值中最高位为 1 的位置索引值。
  // 2^i = 1000***  即，第 i 位为 1，其余为 0 的二进制整型值。
  for (int i = 4; i >= 0; --i) {
    int shift = (1 << i);
    uint32_t x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  DCHECK_EQ(value, 1u);
  return log;
}

// Returns the integer i such as 2^(i-1) < n <= 2^i
// 
// 返回 2^i（幂次方）不小于 n 的最小整数值的幂。即，不等式： 2^(i-1) < n <= 2^i
// eg. : n=6; i=3
inline int Log2Ceiling(uint32_t n) {
  if (n == 0) {
    return -1;
  } else {
    // Log2Floor returns -1 for 0, so the following works correctly for n=1.
    return 1 + Log2Floor(n - 1);
  }
}

}  // namespace bits
}  // namespace butil

#endif  // BUTIL_BITS_H_

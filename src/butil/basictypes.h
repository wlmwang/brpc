// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains definitions of our old basic integral types
// ((u)int{8,16,32,64}) and further includes. I recommend that you use the C99
// standard types instead, and include <stdint.h>/<stddef.h>/etc. as needed.
// Note that the macros and macro-like constructs that were formerly defined in
// this file are now available separately in butil/macros.h.
// 
// 此文件包含旧的基本整型类型 ((u)int{8,16,32,64}) 的定义。
// 建议使用 C99 标准类型代替，包括头文件 <stdint.h>/<stddef.h> 即可。

#ifndef BUTIL_BASICTYPES_H_
#define BUTIL_BASICTYPES_H_

#include <limits.h>  // So we can set the bounds of our types.
#include <stddef.h>  // For size_t.
#include <stdint.h>  // For intptr_t.

#include "butil/macros.h"
#include "butil/port.h"  // Types that only need exist on certain systems.

// DEPRECATED: Please use std::numeric_limits (from <limits>) instead.
// 
// 请使用 #include <limits>	# 其中 std::numeric_limits 代替一下定义使用。本质
// 上 std::numeric_limits 是一个 type trait. 编译时定义以下所有值
const uint8_t  kuint8max  = (( uint8_t) 0xFF);
const uint16_t kuint16max = ((uint16_t) 0xFFFF);
const uint32_t kuint32max = ((uint32_t) 0xFFFFFFFF);
const uint64_t kuint64max = ((uint64_t) 0xFFFFFFFFFFFFFFFFULL);
const  int8_t  kint8min   = ((  int8_t) 0x80);
const  int8_t  kint8max   = ((  int8_t) 0x7F);
const  int16_t kint16min  = (( int16_t) 0x8000);
const  int16_t kint16max  = (( int16_t) 0x7FFF);
const  int32_t kint32min  = (( int32_t) 0x80000000);
const  int32_t kint32max  = (( int32_t) 0x7FFFFFFF);
const  int64_t kint64min  = (( int64_t) 0x8000000000000000LL);
const  int64_t kint64max  = (( int64_t) 0x7FFFFFFFFFFFFFFFLL);

#endif  // BUTIL_BASICTYPES_H_

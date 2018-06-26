// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a wrapper around system calls which may be interrupted by a
// signal and return EINTR. See man 7 signal.
// To prevent long-lasting loops (which would likely be a bug, such as a signal
// that should be masked) to go unnoticed, there is a limit after which the
// caller will nonetheless see an EINTR in Debug builds.
//
// On Windows, this wrapper macro does nothing.
//
// Don't wrap close calls in HANDLE_EINTR. Use IGNORE_EINTR if the return
// value of close is significant. See http://crbug.com/269623.
// 
// 系统调用的包装器 HANDLE_EINTR 宏。可忽略被信号中断 EINTR (signal 7)  的系统调用。
// 不要在 HANDLE_EINTR 包装 close 调用 ， 如果 close 返回值是重要的，使用 IGNORE_EINTR
// 
// 注：Windows 系统，不做任何事情。调试模式下，只忽略前 100 次 EINTR 信号中断。

#ifndef BUTIL_POSIX_EINTR_WRAPPER_H_
#define BUTIL_POSIX_EINTR_WRAPPER_H_

#include "butil/build_config.h"
#include "butil/macros.h"   // BAIDU_TYPEOF

#if defined(OS_POSIX)

#include <errno.h>

// @tips
// 内嵌语句表达式 (Statement-embedded expression) 是指在 GUN C 中，用括号将复合语句括
// 起来形成的表达式。他允许你在复合语句中使用循环，跳转和局部变量。在括号括起来的复合语句中，
// 最后的一句必须是一个以分号结尾的表达式。这个表达式代表了整个结构的值。如果你在大括号里的最
// 后一句用的是其他的语句，则整个结构的返回类型为 void，即没有合法的返回值。
// 
// Use like:
// int i = ({int x = 1;x;});  // i = 1
// int i = ({int x,y; int z = 1; x = 2, y = 3;}); // i = 3
// int i = ({int x = 1; int y = 1;}); // error: void value not ignored as it ought to be

#if defined(NDEBUG)

// 忽略 EINTR 信号中断
#define HANDLE_EINTR(x) ({ \
  BAIDU_TYPEOF(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
  } while (eintr_wrapper_result == -1 && errno == EINTR); \
  eintr_wrapper_result; \
})

#else

// 调试模式下，只忽略前 100 次 EINTR 信号中断
#define HANDLE_EINTR(x) ({ \
  int eintr_wrapper_counter = 0; \
  BAIDU_TYPEOF(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
  } while (eintr_wrapper_result == -1 && errno == EINTR && \
           eintr_wrapper_counter++ < 100); \
  eintr_wrapper_result; \
})

#endif  // NDEBUG

#define IGNORE_EINTR(x) ({ \
  BAIDU_TYPEOF(x) eintr_wrapper_result;     \
  do { \
    eintr_wrapper_result = (x); \
    if (eintr_wrapper_result == -1 && errno == EINTR) { \
      eintr_wrapper_result = 0; \
    } \
  } while (0); \
  eintr_wrapper_result; \
})

#else

// Windows 系统，不做任何事情
#define HANDLE_EINTR(x) (x)
#define IGNORE_EINTR(x) (x)

#endif  // OS_POSIX

#endif  // BUTIL_POSIX_EINTR_WRAPPER_H_

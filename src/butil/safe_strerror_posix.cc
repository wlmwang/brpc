// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/build_config.h"
#include "butil/safe_strerror_posix.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(__GLIBC__) || defined(OS_NACL)
# define USE_HISTORICAL_STRERRO_R 1
#else
# define USE_HISTORICAL_STRERRO_R 0
#endif

#if USE_HISTORICAL_STRERRO_R && defined(__GNUC__)
// GCC will complain about the unused second wrap function unless we tell it
// that we meant for them to be potentially unused, which is exactly what this
// attribute is for.
// 
// @tips
// 除非我们告诉 GCC 编译器，否则 GCC 将 "抱怨" 我们声明的未使用的函数。
#define POSSIBLY_UNUSED __attribute__((unused))
#else
#define POSSIBLY_UNUSED
#endif

#if USE_HISTORICAL_STRERRO_R
// glibc has two strerror_r functions: a historical GNU-specific one that
// returns type char *, and a POSIX.1-2001 compliant one available since 2.3.4
// that returns int. This wraps the GNU-specific one.
// 
// glibc 有两个 strerror_r() 函数版本：
// 1. 一个是返回类型为 char * （历史 GNU-specific）版本。
// 2. 另一个是自 2.3.4 起，返回 int 的与 POSIX.1-2001 兼容版本。
// 
// 
// GNU-specific 版本的 strerror_r() 包装器。
static void POSSIBLY_UNUSED wrap_posix_strerror_r(
    char *(*strerror_r_ptr)(int, char *, size_t),
    int err,
    char *buf,
    size_t len) {
  // GNU version.
  char *rc = (*strerror_r_ptr)(err, buf, len);
  if (rc != buf) {
    // glibc did not use buf and returned a static string instead. Copy it
    // into buf.
    // 
    // glibc 没有使用 buf 而是返回了一个静态字符串。将其复制到 buf 中。
    buf[0] = '\0';
    strncat(buf, rc, len - 1);
  }
  // The GNU version never fails. Unknown errors get an "unknown error" message.
  // The result is always null terminated.
  // 
  // GNU 版本永远不会失败。未知错误会收到 “unknown error” 消息。结果始终为 null 结尾。
}
#endif  // USE_HISTORICAL_STRERRO_R

// Wrapper for strerror_r functions that implement the POSIX interface. POSIX
// does not define the behaviour for some of the edge cases, so we wrap it to
// guarantee that they are handled. This is compiled on all POSIX platforms, but
// it will only be used on Linux if the POSIX strerror_r implementation is
// being used (see below).
// 
// 实现 POSIX 接口的 strerror_r 函数的包装器。 POSIX 没有定义某些边缘情况的行为，因此
// 我们将其包装以保证它们得到处理。这是在所有 POSIX 平台上编译的，但只有在使用 
// POSIX strerror_r 实现时才会在 Linux上 使用（见下文）。
static void POSSIBLY_UNUSED wrap_posix_strerror_r(
    int (*strerror_r_ptr)(int, char *, size_t),
    int err,
    char *buf,
    size_t len) {
  int old_errno = errno;
  // Have to cast since otherwise we get an error if this is the GNU version
  // (but in such a scenario this function is never called). Sadly we can't use
  // C++-style casts because the appropriate one is reinterpret_cast but it's
  // considered illegal to reinterpret_cast a type to itself, so we get an
  // error in the opposite case.
  // 
  // 必须进行强制转换，否则我们会遇到错误，如果这是 GNU 版本（但在这种情况下，永远不会调
  // 用此函数）。
  int result = (*strerror_r_ptr)(err, buf, len);
  if (result == 0) {
    // POSIX is vague about whether the string will be terminated, although
    // it indirectly implies that typically ERANGE will be returned, instead
    // of truncating the string. We play it safe by always terminating the
    // string explicitly.
    // 
    // POSIX 对字符串是否将被终止是模糊的，尽管它间接暗示通常会返回 ERANGE ，而不是截
    // 断字符串。我们通过总是明确地终止字符串来保证安全。
    buf[len - 1] = '\0';
  } else {
    // Error. POSIX is vague about whether the return value is itself a system
    // error code or something else. On Linux currently it is -1 and errno is
    // set. On BSD-derived systems it is a system error and errno is unchanged.
    // We try and detect which case it is so as to put as much useful info as
    // we can into our message.
    // 
    // 失败时，POSIX 对于返回值本身是系统错误代码还是其他东西都很模糊。在 Linux 上，它目
    // 前是 -1 并且设置了 errno 。在 BSD 派生的系统上，它是系统错误， errno 不变。我们
    // 尝试检测它是哪种情况，以便尽可能多地将有用信息放入我们的信息中。
    // 
    // 在 strerror_r() 中遇到错误码
    int strerror_error;  // The error encountered in strerror
    int new_errno = errno;
    if (new_errno != old_errno) {
      // errno was changed, so probably the return value is just -1 or something
      // else that doesn't provide any info, and errno is the error.
      // 
      // errno 更改，因此可能返回值仅为 -1 或其他不提供任何信息的内容，并且 errno 是错误。
      strerror_error = new_errno;
    } else {
      // Either the error from strerror_r was the same as the previous value, or
      // errno wasn't used. Assume the latter.
      // 
      // strerror_r 中的错误与先前的值相同，或者未使用 errno 。假设后者。
      strerror_error = result;
    }
    // snprintf truncates and always null-terminates.
    // 
    // snprintf 截断并始终为 null 结尾。
    snprintf(buf,
             len,
             "Error %d while retrieving error %d",
             strerror_error,
             err);
  }
  errno = old_errno;
}

void safe_strerror_r(int err, char *buf, size_t len) {
  if (buf == NULL || len <= 0) {
    return;
  }
  // If using glibc (i.e., Linux), the compiler will automatically select the
  // appropriate overloaded function based on the function type of strerror_r.
  // The other one will be elided from the translation unit since both are
  // static.
  // 
  // 如果使用 glibc（即 Linux），编译器将根据 strerror_r 的函数类型自动选择适当的重载
  // 函数。另一个将从翻译单元中"删除"（始终不被匹配调用到），因为两者都是静态函数的。
  wrap_posix_strerror_r(&strerror_r, err, buf, len);
}

std::string safe_strerror(int err) {
  // 声明足够长的错误缓冲区
  const int buffer_size = 256;
  char buf[buffer_size];
  safe_strerror_r(err, buf, sizeof(buf));
  // 转换成 std::string
  return std::string(buf);
}

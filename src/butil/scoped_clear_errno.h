// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_SCOPED_CLEAR_ERRNO_H_
#define BUTIL_SCOPED_CLEAR_ERRNO_H_

#include <errno.h>

#include "butil/basictypes.h"

namespace butil {

// Simple scoper that saves the current value of errno, resets it to 0, and on
// destruction puts the old value back.
// 
// 简单的 errno 作用域管理类。构造时保存当前值，将其重置为 0 ，并在销毁时将旧值恢复（如果
// 期间 errno 没有变化）。（与 AutoReset<> 类功能较为相似）。
class ScopedClearErrno {
 public:
  ScopedClearErrno() : old_errno_(errno) {
    errno = 0;
  }
  ~ScopedClearErrno() {
    if (errno == 0)
      errno = old_errno_;
  }

 private:
  const int old_errno_;

  DISALLOW_COPY_AND_ASSIGN(ScopedClearErrno);
};

}  // namespace butil

#endif  // BUTIL_SCOPED_CLEAR_ERRNO_H_

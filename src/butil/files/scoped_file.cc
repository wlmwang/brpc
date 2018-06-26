// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/files/scoped_file.h"

#include "butil/logging.h"

#if defined(OS_POSIX)
#include <unistd.h>

#include "butil/posix/eintr_wrapper.h"
#endif

namespace butil {
namespace internal {

#if defined(OS_POSIX)

// static
void ScopedFDCloseTraits::Free(int fd) {
  // It's important to crash here.
  // There are security implications to not closing a file descriptor
  // properly. As file descriptors are "capabilities", keeping them open
  // would make the current process keep access to a resource. Much of
  // Chrome relies on being able to "drop" such access.
  // It's especially problematic on Linux with the setuid sandbox, where
  // a single open directory would bypass the entire security model.
  // 
  // 在这里崩溃很重要：没有正确关闭文件描述符有安全隐患（会造成 capabilities 机制
  // 混乱。访问不该访问而没有正确关闭的资源）。
  // 由于文件描述符是 "capabilities" ，因此保持打开状态将使当前进程能够访问资源。
  // Chrome 的大部分依赖于能够 "删除" 这样的访问。在使用 setuid 沙箱的 Linux 上
  // 尤其成问题，单个打开的目录将绕过整个安全模型。
  PCHECK(0 == IGNORE_EINTR(close(fd)));
}

#endif  // OS_POSIX

}  // namespace internal
}  // namespace butil

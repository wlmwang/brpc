// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/rand_util.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "butil/file_util.h"
#include "butil/lazy_instance.h"
#include "butil/logging.h"

namespace {

// We keep the file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive), and since we may not even be able to reopen
// it if we are later put in a sandbox. This class wraps the file descriptor so
// we can use LazyInstance to handle opening it on the first access.
// 
// 实例化 URandomFd 时保存 /dev/urandom 的打开文件描述符，以使我们不重复打开它。因为如
// 果我们将其放入 sandbox 中，我们可能甚至不能重复打开它。
// 
// 该类包装文件描述符，可以使用 LazyInstance<> 来延迟到我们第一次访问时在打开它。
class URandomFd {
 public:
  URandomFd() : fd_(open("/dev/urandom", O_RDONLY)) {
    DCHECK_GE(fd_, 0) << "Cannot open /dev/urandom: " << errno;
  }

  ~URandomFd() { close(fd_); }

  int fd() const { return fd_; }

 private:
  const int fd_;
};

butil::LazyInstance<URandomFd>::Leaky g_urandom_fd = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace butil {

// NOTE: This function must be cryptographically secure. http://crbug.com/140076
uint64_t RandUint64() {
  uint64_t number;
  RandBytes(&number, sizeof(number));
  return number;
}

// 使用 |output_length| 长度随机数据填充到 |output| 中。
void RandBytes(void* output, size_t output_length) {
  const int urandom_fd = g_urandom_fd.Pointer()->fd();
  // 获取文件描述符 urandom_fd 指定字节长度内容
  const bool success =
      ReadFromFD(urandom_fd, static_cast<char*>(output), output_length);
  CHECK(success);
}

int GetUrandomFD(void) {
  return g_urandom_fd.Pointer()->fd();
}

}  // namespace butil

// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_FILE_DESCRIPTOR_POSIX_H_
#define BUTIL_FILE_DESCRIPTOR_POSIX_H_

#include "butil/files/file.h"

namespace butil {

// -----------------------------------------------------------------------------
// We introduct a special structure for file descriptors in order that we are
// able to use template specialisation to special-case their handling.
//
// WARNING: (Chromium only) There are subtleties to consider if serialising
// these objects over IPC. See comments in ipc/ipc_message_utils.h
// above the template specialisation for this structure.
// 
// 我们引入了一个特殊的文件描述符结构，以便我们能够使用模板特化来处理它们的特殊情况。
// 
// 警告:( Chromium only ）如果在 IPC 上串行化这些对象，可以考虑一些细节。请参阅该结构
// 的模板特化之上的 ipc/ipc_message_utils.h 中的注释。
// -----------------------------------------------------------------------------
struct FileDescriptor {
  FileDescriptor() : fd(-1), auto_close(false) {}

  FileDescriptor(int ifd, bool iauto_close) : fd(ifd), auto_close(iauto_close) {
  }

  FileDescriptor(File file) : fd(file.TakePlatformFile()), auto_close(true) {}

  bool operator==(const FileDescriptor& other) const {
    return (fd == other.fd && auto_close == other.auto_close);
  }

  bool operator!=(const FileDescriptor& other) const {
    return !operator==(other);
  }

  // A comparison operator so that we can use these as keys in a std::map.
  // 
  // 比较运算符，以便我们可以将其用作 std::map 中的键。
  bool operator<(const FileDescriptor& other) const {
    return other.fd < fd;
  }

  int fd;
  // If true, this file descriptor should be closed after it has been used. For
  // example an IPC system might interpret this flag as indicating that the
  // file descriptor it has been given should be closed after use.
  // 
  // 如果为 true ，则该文件描述符在使用后应该关闭。例如， IPC 系统可能会将此标志解释为，表
  // 明它已经给出的文件描述符应在使用后关闭。
  bool auto_close;
};

}  // namespace butil

#endif  // BUTIL_FILE_DESCRIPTOR_POSIX_H_

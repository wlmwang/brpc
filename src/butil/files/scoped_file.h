// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ScopedFD 用于管理底层文件描述符。 
// ScopedFILE 用户管理文件句柄，即带缓冲 I/O 接口（带有打开/关闭功能）。

#ifndef BUTIL_FILES_SCOPED_FILE_H_
#define BUTIL_FILES_SCOPED_FILE_H_

#include <stdio.h>

#include "butil/base_export.h"
#include "butil/logging.h"
#include "butil/memory/scoped_ptr.h"
#include "butil/scoped_generic.h"
#include "butil/build_config.h"

namespace butil {

namespace internal {

// ScopedGeneric 模板的 trait 特性（文件描述符）
#if defined(OS_POSIX)
struct BUTIL_EXPORT ScopedFDCloseTraits {
  // 非法文件描述符
  static int InvalidValue() {
    return -1;
  }
  // 销毁文件描述符
  static void Free(int fd);
};
#endif

}  // namespace internal

// -----------------------------------------------------------------------------

#if defined(OS_POSIX)
// A low-level Posix file descriptor closer class. Use this when writing
// platform-specific code, especially that does non-file-like things with the
// FD (like sockets).
//
// If you're writing low-level Windows code, see butil/win/scoped_handle.h
// which provides some additional functionality.
//
// If you're writing cross-platform code that deals with actual files, you
// should generally use butil::File instead which can be constructed with a
// handle, and in addition to handling ownership, has convenient cross-platform
// file manipulation functions on it.
// 
// 低级 Posix 平台文件描述符 closer（关闭） 类。在编写特定于平台的代码时使用此功能，特别是
// 使用 FD（如套接字）非文件化的东西。
//
// 如果你正在编写涉及实际文件的跨平台代码，那么通常应该使用 butil::File 的句柄，除了处理所
// 有权之外，还可以在其上方便的使用跨平台文件操作功能。
typedef ScopedGeneric<int, internal::ScopedFDCloseTraits> ScopedFD;
#endif

//// Automatically closes |FILE*|s.
///
/// 自动关闭 |FILE*| 类。该类只能移动操作。
class ScopedFILE {
    MOVE_ONLY_TYPE_FOR_CPP_03(ScopedFILE, RValue);
public:
    ScopedFILE() : _fp(NULL) {}

    // Open file at |path| with |mode|.
    // If fopen failed, operator FILE* returns NULL and errno is set.
    // 
    // 以 |mode| 模式打开 |path| 文件句柄。
    // 打开失败， _fp 为空(operator FILE* 返回 NULL)，并设置 errno
    ScopedFILE(const char *path, const char *mode) {
        _fp = fopen(path, mode);
    }

    // |fp| must be the return value of fopen as we use fclose in the
    // destructor, otherwise the behavior is undefined
    // 
    // fp 必须是 fopen 的返回值，否则 fclose 将会有 ub
    explicit ScopedFILE(FILE *fp) :_fp(fp) {}

    // 移动构造
    ScopedFILE(RValue rvalue) {
        _fp = rvalue.object->_fp;
        rvalue.object->_fp = NULL;
    }

    ~ScopedFILE() {
        if (_fp != NULL) {
            fclose(_fp);
            _fp = NULL;
        }
    }

    // Close current opened file and open another file at |path| with |mode|
    // 
    // 关闭当前文件句柄。以 |mode| 模式打开 |path| 新文件句柄。
    void reset(const char* path, const char* mode) {
        reset(fopen(path, mode));
    }

    void reset() { reset(NULL); }

    void reset(FILE *fp) {
        if (_fp != NULL) {
            fclose(_fp);
            _fp = NULL;
        }
        _fp = fp;
    }

    // Set internal FILE* to NULL and return previous value.
    // 
    // 释放当前文件句柄所有权
    FILE* release() {
        FILE* const prev_fp = _fp;
        _fp = NULL;
        return prev_fp;
    }
    
    operator FILE*() const { return _fp; }

    FILE* get() { return _fp; }
    
private:
    FILE* _fp;
};

}  // namespace butil

#endif  // BUTIL_FILES_SCOPED_FILE_H_

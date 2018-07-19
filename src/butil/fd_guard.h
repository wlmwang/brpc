// Copyright (c) 2011 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Ge,Jun (gejun@baidu.com)
// Date: Mon. Nov 7 14:47:36 CST 2011

#ifndef BUTIL_FD_GUARD_H
#define BUTIL_FD_GUARD_H

#include <unistd.h>                                  // close()

namespace butil {

// RAII file descriptor.
//
// Example:
//    fd_guard fd1(open(...));
//    if (fd1 < 0) {
//        printf("Fail to open\n");
//        return -1;
//    }
//    if (another-error-happened) {
//        printf("Fail to do sth\n");
//        return -1;   // *** closing fd1 automatically ***
//    }
//    
// 文件描述符 fd 的 RAII 包装器。
class fd_guard {
public:
    fd_guard() : _fd(-1) {}
    explicit fd_guard(int fd) : _fd(fd) {}
    
    ~fd_guard() {
        if (_fd >= 0) {
            ::close(_fd);
            _fd = -1;
        }
    }

    // Close current fd and replace with another fd
    // 
    // 关闭当前 fd 并替换为新的 fd
    void reset(int fd) {
        if (_fd >= 0) {
            ::close(_fd);
            _fd = -1;
        }
        _fd = fd;
    }

    // Set internal fd to -1 and return the value before set.
    // 
    // 将内部 fd 设置为 -1 ，并返回该 fd 。即，释放 fd 所有权。
    int release() {
        const int prev_fd = _fd;
        _fd = -1;
        return prev_fd;
    }
    
    // int 隐式类型转换
    operator int() const { return _fd; }
    
private:
    // Copying this makes no sense.
    // 
    // 禁止拷贝/赋值
    fd_guard(const fd_guard&);
    void operator=(const fd_guard&);
    
    int _fd;
};

}  // namespace butil

#endif  // BUTIL_FD_GUARD_H

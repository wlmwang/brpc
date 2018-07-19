// Copyright (c) 2010 Baidu, Inc.
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
// Date: Fri Sep 10 13:34:25 CST 2010

#include "butil/build_config.h"                        // OS_MACOSX
#include <errno.h>                                     // errno
#include <string.h>                                    // strerror_r
#include <stdlib.h>                                    // EXIT_FAILURE
#include <stdio.h>                                     // snprintf
#include <pthread.h>                                   // pthread_mutex_t
#include <unistd.h>                                    // _exit
#include "butil/scoped_lock.h"                         // BAIDU_SCOPED_LOCK

namespace butil {

// 错误码范围限制
const int ERRNO_BEGIN = -32768;
const int ERRNO_END = 32768;
// 全局静态错误描述字符串数组 char*[65536]
static const char* errno_desc[ERRNO_END - ERRNO_BEGIN] = {};
static pthread_mutex_t modify_desc_mutex = PTHREAD_MUTEX_INITIALIZER;

// 库函数 strerror_r() 获取系统错误码的错误描述使用内存。
const size_t ERROR_BUFSIZE = 64;
__thread char tls_error_buf[ERROR_BUFSIZE];

// 注册用户定制化的错误码及描述。
// 
// Use like:
// DescribeCustomizedErrno(EMYERROR, "EMYERROR", "my error");
int DescribeCustomizedErrno(
    int error_code, const char* error_name, const char* description) {
    BAIDU_SCOPED_LOCK(modify_desc_mutex);
    if (error_code < ERRNO_BEGIN || error_code >= ERRNO_END) {
        // error() is a non-portable GNU extension that should not be used.
        fprintf(stderr, "Fail to define %s(%d) which is out of range, abort.",
              error_name, error_code);
        _exit(1);
    }
    const char* desc = errno_desc[error_code - ERRNO_BEGIN];
    if (desc) {
        // 已注册错误码，并且错误描述相同，打印 WARNING 并返回。
        if (strcmp(desc, description) == 0) {
            fprintf(stderr, "WARNING: Detected shared library loading\n");
            return -1;
        }
    } else {
        // 与系统已存在的合法的错误码重合，打印 Fail 并退出程序。
#if defined(OS_MACOSX)
        const int rc = strerror_r(error_code, tls_error_buf, ERROR_BUFSIZE);
        if (rc != EINVAL)
#else
        desc = strerror_r(error_code, tls_error_buf, ERROR_BUFSIZE);
        if (desc && strncmp(desc, "Unknown error", 13) != 0)
#endif
        {
            fprintf(stderr, "Fail to define %s(%d) which is already defined as `%s', abort.",
                    error_name, error_code, desc);
            _exit(1);
        }
    }
    // 添加错误码及对应错误描述。
    errno_desc[error_code - ERRNO_BEGIN] = description;
    return 0;  // must
}

}  // namespace butil

const char* berror(int error_code) {
    if (error_code == -1) {
        return "General error -1";
    }
    if (error_code >= butil::ERRNO_BEGIN && error_code < butil::ERRNO_END) {
        const char* s = butil::errno_desc[error_code - butil::ERRNO_BEGIN];
        if (s) {
            return s;
        }
        // 尝试获取系统错误描述
#if defined(OS_MACOSX)
        const int rc = strerror_r(error_code, butil::tls_error_buf, butil::ERROR_BUFSIZE);
        if (rc == 0 || rc == ERANGE/*bufsize is not long enough*/) {
            return butil::tls_error_buf;
        }
#else
        s = strerror_r(error_code, butil::tls_error_buf, butil::ERROR_BUFSIZE);
        if (s) {
            return s;
        }
#endif
    }
    // 未定义错误描述。
    snprintf(butil::tls_error_buf, butil::ERROR_BUFSIZE,
             "Unknown error %d", error_code);
    return butil::tls_error_buf;
}

// 当前错误描述
const char* berror() {
    return berror(errno);
}

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

// Add customized errno.
// 
// 可定制添加错误码及错误描述

#ifndef BUTIL_BAIDU_ERRNO_H
#define BUTIL_BAIDU_ERRNO_H

#define __const__
#include <errno.h>                           // errno
#include "butil/macros.h"                     // BAIDU_CONCAT

//-----------------------------------------
// Use system errno before defining yours !
//-----------------------------------------
//
// To add new errno, you shall define the errno in header first, either by
// macro or constant, or even in protobuf.
//
//     #define ESTOP -114                // C/C++
//     static const int EMYERROR = 30;   // C/C++
//     const int EMYERROR2 = -31;        // C++ only
//
// Then you can register description of the error by calling
// BAIDU_REGISTER_ERRNO(the_error_number, its_description) in global scope of
// a .cpp or .cc files which will be linked.
// 
//     BAIDU_REGISTER_ERRNO(ESTOP, "the thread is stopping")
//     BAIDU_REGISTER_ERRNO(EMYERROR, "my error")
//
// Once the error is successfully defined:
//     berror(error_code) returns the description.
//     berror() returns description of last system error code.
//
// %m in printf-alike functions does NOT recognize errors defined by
// BAIDU_REGISTER_ERRNO, you have to explicitly print them by %s.
//
//     errno = ESTOP;
//     printf("Something got wrong, %m\n");            // NO
//     printf("Something got wrong, %s\n", berror());  // YES
//
// When the error number is re-defined, a linking error will be reported:
// 
//     "redefinition of `class BaiduErrnoHelper<30>'"
//
// Or the program aborts at runtime before entering main():
// 
//     "Fail to define EMYERROR(30) which is already defined as `Read-only file system', abort"
//
// 
// 可添加注册自定义错误号及其描述，不能重复注册同一个错误号（链接错误）。描述信息必须是
// 全局、常量字符串（不能是局部的）。并且不能注册与系统重合的错误信息，否则程序在运行时，
// 进入 main() 之前会报错（运行错误）。
// 
// 	*.h （全局）
//     #define ESTOP -114                // C/C++
//     static const int EMYERROR = 30;   // C/C++
//     const int EMYERROR2 = -31;        // C++ only
//
// *.cpp （全局）
//     BAIDU_REGISTER_ERRNO(ESTOP, "the thread is stopping");
//     BAIDU_REGISTER_ERRNO(EMYERROR, "my error");
//
//	printf("description of ESTOP error code, %s\n", berror(ESTOP));
//	printf("description of last system error code, %s\n", berror());
//	

namespace butil {
// You should not call this function, use BAIDU_REGISTER_ERRNO instead.
// 
// 注册用户定制化的错误码及描述
extern int DescribeCustomizedErrno(int, const char*, const char*);
}

// 控制不能重复注册 (BAIDU_REGISTER_ERRNO) 相同错误码。重复注册时：会生成两个相同的 
// BaiduErrnoHelper<error_code> 偏特化版本，目标文件链接时会报错。
template <int error_code> class BaiduErrnoHelper {};

// 注册自定义错误码 errno 及对应描述字符串 description
#define BAIDU_REGISTER_ERRNO(error_code, description)                   \
    const int ALLOW_UNUSED BAIDU_CONCAT(baidu_errno_dummy_, __LINE__) =              \
        ::butil::DescribeCustomizedErrno((error_code), #error_code, (description)); \
    template <> class BaiduErrnoHelper<(int)(error_code)> {};

// 返回对应错误码的描述。
const char* berror(int error_code);
// 返回当前错误描述。
const char* berror();

#endif  //BUTIL_BAIDU_ERRNO_H

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

#include <cxxabi.h>                              // __cxa_demangle
#include <string>                                // std::string
#include <stdlib.h>                              // free()

namespace butil {

// Try to convert mangled |name| to human-readable name.
// Returns:
//   |name|    -  Fail to demangle |name|
//   otherwise -  demangled name
//   
// 转换 |name| 为人类可读类名字符串。 |name| = typeid(T).name() 为 C++ ABI 规
// 则下的有效名称。
std::string demangle(const char* name) {
    // mangled_name
    //   A NULL-terminated character string containing the name to
    //   be demangled.
    // output_buffer:
    //   A region of memory, allocated with malloc, of *length bytes,
    //   into which the demangled name is stored. If output_buffer is
    //   not long enough, it is expanded using realloc. output_buffer
    //   may instead be NULL; in that case, the demangled name is placed
    //   in a region of memory allocated with malloc.
    // length
    //   If length is non-NULL, the length of the buffer containing the
    //   demangled name is placed in *length.
    // status
    //   *status is set to one of the following values:
    //    0: The demangling operation succeeded.
    //   -1: A memory allocation failure occurred.
    //   -2: mangled_name is not a valid name under the C++ ABI
    //       mangling rules.
    //   -3: One of the arguments is invalid.
    //   
    // 
    // @tips
    // char *__cxa_demangle(const char *mangled_name, char *output_buffer, size_t *length, 
    // int *status);
    // |mangled_name| 
    //      一个以 NULL 结尾的 c-style 字符串，包含要解析的名称。
    // |output_buffer| 
    //      一个用 malloc() 分配 *length 长度的内存区域，用来存储 |mangled_name| 被转换后的名字。
    //      如果 |output_buffer| 不够长，则使用 realloc 进行扩展。 |output_buffer| 可以为 NULL;
    //      在这种情况下，被解析的名称将被放置在用 malloc() 分配的内存区域中。
    // |length|
    //      如果 |length| 不为 NULL ，|mangled_name| 缓冲区的长度是 *length
    // |status|
    //      0: 解析名称成功
    //      1: 内存分配失败
    //      2: mangled_name 不是 C++ ABI 规则下的有效名称
    //      3: 某个参数非法
    int status = 0;
    char* buf = abi::__cxa_demangle(name, NULL, NULL, &status);
    if (status == 0) {
        std::string s(buf);
        // 拷贝后，释放 malloc() 分配的内存。
        free(buf);
        return s;
    }
    // 失败时，返回原始 |name| 名称字符串
    return std::string(name);
}

}  // namespace butil


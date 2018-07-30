// Copyright (c) 2012 Baidu, Inc.
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
// Date: Thu Nov 22 13:57:56 CST 2012

#include "butil/macros.h"
#include "butil/zero_copy_stream_as_streambuf.h"

namespace butil {

BAIDU_CASSERT(sizeof(std::streambuf::char_type) == sizeof(char),
              only_support_char);

// 缓冲区写满。字符 ch 是调用 overflow 时当前的字符
int ZeroCopyStreamAsStreamBuf::overflow(int ch) {
    if (ch == std::streambuf::traits_type::eof()) {
        return ch;
    }
    void* block = NULL;
    int size = 0;
    // 由 _zero_copy_stream 再次分配一个可用内存块，用于输出缓冲区
    if (_zero_copy_stream->Next(&block, &size)) {
        setp((char*)block, (char*)block + size);
        // if size == 1, this function will call overflow again.
        // 
        // 压入当前 ch 字符
        return sputc(ch);
    } else {
        // 返回 EOF 失败
        setp(NULL, NULL);
        return std::streambuf::traits_type::eof();
    }
}

int ZeroCopyStreamAsStreamBuf::sync() {
    // data are already in IOBuf.
    // 
    // 字符是直接写入 _zero_copy_stream->Next() 分配的内存，无需拷贝。
    return 0;
}

ZeroCopyStreamAsStreamBuf::~ZeroCopyStreamAsStreamBuf() {
    shrink();
}

void ZeroCopyStreamAsStreamBuf::shrink() {
    if (pbase() != NULL) {
        // 把未使用的内存归还
        _zero_copy_stream->BackUp(epptr() - pptr());
        setp(NULL, NULL);
    }
}

// 根据相对位置移动内部指针
std::streampos ZeroCopyStreamAsStreamBuf::seekoff(
    std::streamoff off,
    std::ios_base::seekdir way,
    std::ios_base::openmode which) {
    if (off == 0 && way == std::ios_base::cur) {
        // 获取当前偏移量
        return _zero_copy_stream->ByteCount() - (epptr() - pptr());
    }
    return (std::streampos)(std::streamoff)-1;
}


}  // namespace butil

// Copyright (c) 2014 Baidu, Inc.
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

#ifndef BUTIL_RAW_PACK_H
#define BUTIL_RAW_PACK_H

#include "butil/sys_byteorder.h"

namespace butil {

// -------------------------------------------------------------------------
// NOTE: RawPacker/RawUnpacker is used for packing/unpacking low-level and
// hard-to-change header. If the fields are likely to be changed in future,
// use protobuf.
// -------------------------------------------------------------------------
// 
// 注意: RawPacker/RawUnpacker 被用于 packing/unpacking 不易更改的低级的 header
// 如果将来可能会更改字段，请使用 protobuf 。

// This utility class packs 32-bit and 64-bit integers into binary data 
// that can be unpacked by RawUnpacker. Notice that the packed data is 
// schemaless and user must match pack..() methods with same-width 
// unpack..() methods to get the integers back.
// Example:
//   char buf[16];  // 4 + 8 + 4 bytes.
//   butil::RawPacker(buf).pack32(a).pack64(b).pack32(c);  // buf holds packed data
//
//   ... network ...
//
//   // positional correspondence with pack..()
//   butil::Unpacker(buf2).unpack32(a).unpack64(b).unpack32(c);
//   
// 
// 此实用类将 32-bit 和 64-bit 整数打包成可由 RawUnpacker 解压缩的二进制数据。
// 注意：打包数据是无模式的。用户必须使用与打包时的 pack..() 方法的相同宽度的 
// unpack..() 方法来解压获取原始整数。
// 
// Example:
//   char buf[16];  // 4 + 8 + 4 bytes.
//   butil::RawPacker(buf).pack32(a).pack64(b).pack32(c);  // 打包数据到 buf
//
//   ... network ...
//
//   // positional correspondence with pack..()
//   butil::Unpacker(buf2).unpack32(a).unpack64(b).unpack32(c);
// 
// 
// 打包 32-bit 和 64-bit 整数，从主机序到网络序转换。
class RawPacker {
public:
    // Notice: User must guarantee `stream' is as long as the packed data.
    // 
    // 用户必须保证 |stream| 与打包后的数据一样长。
    explicit RawPacker(void* stream) : _stream((char*)stream) {}
    ~RawPacker() {}

    // Not using operator<< because some values may be packed differently from
    // its type.
    // 
    // 不能使用 operator<< ，因为某些值打包后可能与其类型不同。
    RawPacker& pack32(uint32_t host_value) {
        // 从主机序转换到网络序
        *(uint32_t*)_stream = HostToNet32(host_value);
        _stream += 4;
        return *this;
    }

    RawPacker& pack64(uint64_t host_value) {
        // 64-bit 分段，再从主机序转换到网络字节序。
        uint32_t *p = (uint32_t*)_stream;
        p[0] = HostToNet32(host_value >> 32);
        p[1] = HostToNet32(host_value & 0xFFFFFFFF);
        _stream += 8;
        return *this;
    }

private:
    char* _stream;  // 打包数据缓冲区指针
};

// This utility class unpacks 32-bit and 64-bit integers from binary data
// packed by RawPacker.
// 
// 解压 RawPacker 打包的二进制数据中 32-bit 和 64-bit 整数。
class RawUnpacker {
public:
    explicit RawUnpacker(const void* stream) : _stream((const char*)stream) {}
    ~RawUnpacker() {}

    RawUnpacker& unpack32(uint32_t & host_value) {
        host_value = NetToHost32(*(const uint32_t*)_stream);
        _stream += 4;
        return *this;
    }

    RawUnpacker& unpack64(uint64_t & host_value) {
        const uint32_t *p = (const uint32_t*)_stream;
        host_value = (((uint64_t)NetToHost32(p[0])) << 32) | NetToHost32(p[1]);
        _stream += 8;
        return *this;
    }

private:
    const char* _stream;    // 打包的数据缓冲区指针
};

}  // namespace butil

#endif  // BUTIL_RAW_PACK_H

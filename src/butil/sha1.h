// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_SHA1_H_
#define BUTIL_SHA1_H_

#include <string>

#include "butil/base_export.h"

namespace butil {

// These functions perform SHA-1 operations.

// SHA-1 哈希的字节长度。
static const size_t kSHA1Length = 20;  // Length in bytes of a SHA-1 hash.

// Computes the SHA-1 hash of the input string |str| and returns the full
// hash.
// 
// 计算输入字符串 |str| 的 SHA-1 hash 散列，并返回完整的 hash 散列。
BUTIL_EXPORT std::string SHA1HashString(const std::string& str);

// Computes the SHA-1 hash of the |len| bytes in |data| and puts the hash
// in |hash|. |hash| must be kSHA1Length bytes long.
// 
// 计算 |len| 长度 |data| 的 SHA-1 散列，并将散列放在 |hash| 中。其中 |hash| 必
// 须是 kSHA1Length 字节长。
BUTIL_EXPORT void SHA1HashBytes(const unsigned char* data, size_t len,
                               unsigned char* hash);

}  // namespace butil

#endif  // BUTIL_SHA1_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/files/file_path.h"

namespace butil {

// 路径名分隔符
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
const FilePath::CharType FilePath::kSeparators[] = FILE_PATH_LITERAL("\\/");
#else  // FILE_PATH_USES_WIN_SEPARATORS
const FilePath::CharType FilePath::kSeparators[] = FILE_PATH_LITERAL("/");
#endif  // FILE_PATH_USES_WIN_SEPARATORS

// 分隔符长度
const size_t FilePath::kSeparatorsLength = arraysize(kSeparators);

// 相对路径目录特殊符
const FilePath::CharType FilePath::kCurrentDirectory[] = FILE_PATH_LITERAL(".");
const FilePath::CharType FilePath::kParentDirectory[] = FILE_PATH_LITERAL("..");

// 文件扩展名分隔符
const FilePath::CharType FilePath::kExtensionSeparator = FILE_PATH_LITERAL('.');

}  // namespace butil

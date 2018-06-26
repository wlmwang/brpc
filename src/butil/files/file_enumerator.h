// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_FILES_FILE_ENUMERATOR_H_
#define BUTIL_FILES_FILE_ENUMERATOR_H_

#include <stack>
#include <vector>

#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/files/file_path.h"
#include "butil/time/time.h"
#include "butil/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace butil {

// A class for enumerating the files in a provided path. The order of the
// results is not guaranteed.
//
// This is blocking. Do not use on critical threads.
//
// Example:
//
//   butil::FileEnumerator enum(my_dir, false, butil::FileEnumerator::FILES,
//                             FILE_PATH_LITERAL("*.txt"));
//   for (butil::FilePath name = enum.Next(); !name.empty(); name = enum.Next())
//     ...
//     
// 
// 用于枚举提供的路径中的文件的类。结果的顺序不能保证（请勿在关键线程上使用）。
// 
// Example:
//
//   butil::FileEnumerator enum(my_dir, false, butil::FileEnumerator::FILES,
//                             FILE_PATH_LITERAL("*.txt"));
//   for (butil::FilePath name = enum.Next(); !name.empty(); name = enum.Next())
//     ...
class BUTIL_EXPORT FileEnumerator {
 public:
  // Note: copy & assign supported.
  // 
  // 文件信息
  class BUTIL_EXPORT FileInfo {
   public:
    FileInfo();
    ~FileInfo();

    bool IsDirectory() const;

    // The name of the file. This will not include any path information. This
    // is in constrast to the value returned by FileEnumerator.Next() which
    // includes the |root_path| passed into the FileEnumerator constructor.
    // 
    // 文件的名称，不包括任何路径信息(basename)，最长 255 字符。这与 FileEnumerator.Next()
    // （包含 |root_path| ）返回的值相反，传递给 FileEnumerator 构造函数。
    FilePath GetName() const;

    // 文件大小
    int64_t GetSize() const;
    // 最后修改时间
    Time GetLastModifiedTime() const;

#if defined(OS_WIN)
    // Note that the cAlternateFileName (used to hold the "short" 8.3 name)
    // of the WIN32_FIND_DATA will be empty. Since we don't use short file
    // names, we tell Windows to omit it which speeds up the query slightly.
    const WIN32_FIND_DATA& find_data() const { return find_data_; }
#elif defined(OS_POSIX)
    // 文件状态信息
    const struct stat& stat() const { return stat_; }
#endif

   private:
    friend class FileEnumerator;

    // 文件状态信息
#if defined(OS_WIN)
    WIN32_FIND_DATA find_data_;
#elif defined(OS_POSIX)
    struct stat stat_;
    FilePath filename_; // 文件名路径
#endif
  };

  // 文件类型
  enum FileType {
    FILES                 = 1 << 0,
    DIRECTORIES           = 1 << 1,
    INCLUDE_DOT_DOT       = 1 << 2,
#if defined(OS_POSIX)
    SHOW_SYM_LINKS        = 1 << 4,
#endif
  };

  // |root_path| is the starting directory to search for. It may or may not end
  // in a slash.
  //
  // If |recursive| is true, this will enumerate all matches in any
  // subdirectories matched as well. It does a breadth-first search, so all
  // files in one directory will be returned before any files in a
  // subdirectory.
  //
  // |file_type|, a bit mask of FileType, specifies whether the enumerator
  // should match files, directories, or both.
  //
  // |pattern| is an optional pattern for which files to match. This
  // works like shell globbing. For example, "*.txt" or "Foo???.doc".
  // However, be careful in specifying patterns that aren't cross platform
  // since the underlying code uses OS-specific matching routines.  In general,
  // Windows matching is less featureful than others, so test there first.
  // If unspecified, this will match all files.
  // NOTE: the pattern only matches the contents of root_path, not files in
  // recursive subdirectories.
  // TODO(erikkay): Fix the pattern matching to work at all levels.
  // 
  // |root_path| 是要搜索的起始目录。它可能会或可能不会以斜线结尾。
  // 
  // 如果 |recursive| 是 true ，将枚举匹配的任何子目录中的所有匹配。它执行广度优先搜索，
  // 因此一个目录中的所有文件将在子目录中的任何文件之前返回。
  // 
  // |file_type| ，文件类型的位掩码，指定枚举器是匹配文件，目录还是两者。
  // 
  // |pattern| 是可供匹配文件的可选模式。与 shell 模式一样。 例如，"*.txt" 或 "Foo???.doc"。
  // 但是，在指定非跨平台的模式时要小心，因为底层代码使用特定于操作系统的匹配例程。一般来说， 
  // Windows 匹配不如其他特性，所以首先在那里测试。如果未指定，则会匹配所有文件。
  // 
  // 注：该模式只匹配 root_path 的内容，而不是递归子目录中的文件。
  // 
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 int file_type);
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 int file_type,
                 const FilePath::StringType& pattern);
  ~FileEnumerator();

  // Returns the next file or an empty string if there are no more results.
  //
  // The returned path will incorporate the |root_path| passed in the
  // constructor: "<root_path>/file_name.txt". If the |root_path| is absolute,
  // then so will be the result of Next().
  // 
  // 如果没有更多结果，则返回下一个文件或空字符串。
  // 
  // 返回的路径将包含 |root_path| 传入构造函数中："<root_path>/file_name.txt" 。
  // 如果 |root_path| 是绝对路径，那么将是 Next() 的结果。
  FilePath Next();

  // Write the file info into |info|.
  FileInfo GetInfo() const;

 private:
  // Returns true if the given path should be skipped in enumeration.
  // 
  // 如果给定路径应在枚举中跳过，则返回 true
  bool ShouldSkip(const FilePath& path);

#if defined(OS_WIN)
  // True when find_data_ is valid.
  bool has_find_data_;
  WIN32_FIND_DATA find_data_;
  HANDLE find_handle_;
#elif defined(OS_POSIX)

  // Read the filenames in source into the vector of DirectoryEntryInfo's
  // 
  // 将 |source| 目录下所有文件项读入 entries 的数组中。
  static bool ReadDirectory(std::vector<FileInfo>* entries,
                            const FilePath& source, bool show_links);

  // The files in the current directory
  // 
  // 当前目录路径下所有文件项数组
  std::vector<FileInfo> directory_entries_;

  // The next entry to use from the directory_entries_ vector
  // 
  // Next() 接口迭代 directory_entries_ 数组的索引
  size_t current_directory_entry_;
#endif

  FilePath root_path_;  // 搜索的起始目录
  bool recursive_;  // 是否递归搜索
  int file_type_; // 文件类型
  FilePath::StringType pattern_;  // Empty when we want to find everything.

  // A stack that keeps track of which subdirectories we still need to
  // enumerate in the breadth-first search.
  // 
  // 待递归广度优先搜索的路径，可能有待枚举的所有子目录。
  std::stack<FilePath> pending_paths_;

  DISALLOW_COPY_AND_ASSIGN(FileEnumerator);
};

}  // namespace butil

#endif  // BUTIL_FILES_FILE_ENUMERATOR_H_

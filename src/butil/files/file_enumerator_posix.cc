// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/files/file_enumerator.h"

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>

#include "butil/logging.h"
#include "butil/threading/thread_restrictions.h"

namespace butil {

// FileEnumerator::FileInfo ----------------------------------------------------

FileEnumerator::FileInfo::FileInfo() {
  memset(&stat_, 0, sizeof(stat_));
}

bool FileEnumerator::FileInfo::IsDirectory() const {
  return S_ISDIR(stat_.st_mode);
}

FilePath FileEnumerator::FileInfo::GetName() const {
  return filename_;
}

int64_t FileEnumerator::FileInfo::GetSize() const {
  return stat_.st_size;
}

butil::Time FileEnumerator::FileInfo::GetLastModifiedTime() const {
  return butil::Time::FromTimeT(stat_.st_mtime);
}

// FileEnumerator --------------------------------------------------------------

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               int file_type)
    : current_directory_entry_(0),
      root_path_(root_path),
      recursive_(recursive),
      file_type_(file_type) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               int file_type,
                               const FilePath::StringType& pattern)
    : current_directory_entry_(0),
      root_path_(root_path),
      recursive_(recursive),
      file_type_(file_type),
      pattern_(root_path.Append(pattern).value()) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  // The Windows version of this code appends the pattern to the root_path,
  // potentially only matching against items in the top-most directory.
  // Do the same here.
  // 
  // 此代码的 Windows 版本将该模式附加到 root_path ，可能仅匹配最顶级目录中的项目。
  if (pattern.empty())
    pattern_ = FilePath::StringType();
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() {
}

FilePath FileEnumerator::Next() {
  ++current_directory_entry_;

  // While we've exhausted the entries in the current directory, do the next
  // 
  // 当前目录路径下所有文件项被迭代结束后，递归搜索子目录。（广度优先搜索的路径）
  while (current_directory_entry_ >= directory_entries_.size()) {
    if (pending_paths_.empty())
      return FilePath();

    // 弹出根目录
    root_path_ = pending_paths_.top();
    root_path_ = root_path_.StripTrailingSeparators();  // 去除结尾 /
    pending_paths_.pop();

    // 读取目录下所有文件项
    std::vector<FileInfo> entries;
    if (!ReadDirectory(&entries, root_path_, file_type_ & SHOW_SYM_LINKS))
      continue;

    // 重置循环标志，以便 Next() 记录迭代位置
    directory_entries_.clear();
    current_directory_entry_ = 0;
    for (std::vector<FileInfo>::const_iterator i = entries.begin();
         i != entries.end(); ++i) {
      FilePath full_path = root_path_.Append(i->filename_);
      if (ShouldSkip(full_path))
        continue;

      // 路径模式匹配
      if (pattern_.size() &&
          fnmatch(pattern_.c_str(), full_path.value().c_str(), FNM_NOESCAPE))
        continue;

      // 子目录加入递归搜索路径
      if (recursive_ && S_ISDIR(i->stat_.st_mode))
        pending_paths_.push(full_path);

      // 当前目录路径下所有文件项数组
      if ((S_ISDIR(i->stat_.st_mode) && (file_type_ & DIRECTORIES)) ||
          (!S_ISDIR(i->stat_.st_mode) && (file_type_ & FILES)))
        directory_entries_.push_back(*i);
    }
  }

  return root_path_.Append(
      directory_entries_[current_directory_entry_].filename_);
}

FileEnumerator::FileInfo FileEnumerator::GetInfo() const {
  return directory_entries_[current_directory_entry_];
}

// 将 |source| 目录下所有文件项读入 entries 的数组中。
bool FileEnumerator::ReadDirectory(std::vector<FileInfo>* entries,
                                   const FilePath& source, bool show_links) {
  butil::ThreadRestrictions::AssertIOAllowed();
  // 打开目录
  DIR* dir = opendir(source.value().c_str());
  if (!dir)
    return false;

#if !defined(OS_LINUX) && !defined(OS_MACOSX) && !defined(OS_BSD) && \
    !defined(OS_SOLARIS) && !defined(OS_ANDROID)
  #error Port warning: depending on the definition of struct dirent, \
         additional space for pathname may be needed
#endif

  struct dirent* dent;
  // readdir_r is marked as deprecated since glibc 2.24. 
  // Using readdir on _different_ DIR* object is already thread-safe in
  // most modern libc implementations.
  // 
  // 自 glibc 2.24 以来， readdir_r 被标记为弃用。在大多数现代 libc 实现中，对 
  // _different_ DIR* 对象使用 readdir 已经是线程安全的。
#if defined(__GLIBC__) &&  \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 24))
  while ((dent = readdir(dir))) {
#else
  struct dirent dent_buf;
  while (readdir_r(dir, &dent_buf, &dent) == 0 && dent) {
#endif
    FileInfo info;
    // 文件的名称，不包括任何路径信息(basename)，最长 255 字符。
    info.filename_ = FilePath(dent->d_name);

    // 返回文件全路径 full_name
    FilePath full_name = source.Append(dent->d_name);
    int ret;
    if (show_links)
      ret = lstat(full_name.value().c_str(), &info.stat_);
    else
      ret = stat(full_name.value().c_str(), &info.stat_);
    if (ret < 0) {
      // Print the stat() error message unless it was ENOENT and we're
      // following symlinks.
      // 
      // 打印 stat() 错误消息，除非它是 ENOENT ，并且显示符号链接
      if (!(errno == ENOENT && !show_links)) {
        DPLOG(ERROR) << "Couldn't stat "
                     << source.Append(dent->d_name).value();
      }
      memset(&info.stat_, 0, sizeof(info.stat_));
    }
    entries->push_back(info);
  }

  closedir(dir);
  return true;
}

}  // namespace butil

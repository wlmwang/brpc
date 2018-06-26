// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/files/file.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "butil/files/file_path.h"
#include "butil/logging.h"
#include "butil/posix/eintr_wrapper.h"
#include "butil/strings/utf_string_conversions.h"
#include "butil/threading/thread_restrictions.h"

#if defined(OS_ANDROID)
#include "butil/os_compat_android.h"
#endif

namespace butil {

// Make sure our Whence mappings match the system headers.
// 
// 确保我们的 Whence 映射与系统头文件定义相匹配
COMPILE_ASSERT(File::FROM_BEGIN   == SEEK_SET &&
               File::FROM_CURRENT == SEEK_CUR &&
               File::FROM_END     == SEEK_END, whence_matches_system);

namespace {

#if defined(OS_BSD) || defined(OS_MACOSX) || defined(OS_NACL)
static int CallFstat(int fd, stat_wrapper_t *sb) {
  butil::ThreadRestrictions::AssertIOAllowed();
  return fstat(fd, sb);
}
#else
static int CallFstat(int fd, stat_wrapper_t *sb) {
  butil::ThreadRestrictions::AssertIOAllowed();
  return fstat64(fd, sb);
}
#endif

// NaCl doesn't provide the following system calls, so either simulate them or
// wrap them in order to minimize the number of #ifdef's in this file.
// 
// NaCl 不提供以下系统调用，因此要么模拟它们，要么包装它们，以便最大限度地减少此文件
// 中 #ifdef 的数量。
#if !defined(OS_NACL)
static bool IsOpenAppend(PlatformFile file) {
  return (fcntl(file, F_GETFL) & O_APPEND) != 0;
}

static int CallFtruncate(PlatformFile file, int64_t length) {
  return HANDLE_EINTR(ftruncate(file, length));
}

static int CallFsync(PlatformFile file) {
  return HANDLE_EINTR(fsync(file));
}

static int CallFutimes(PlatformFile file, const struct timeval times[2]) {
#ifdef __USE_XOPEN2K8
  // futimens should be available, but futimes might not be
  // http://pubs.opengroup.org/onlinepubs/9699919799/

  timespec ts_times[2];
  ts_times[0].tv_sec  = times[0].tv_sec;
  ts_times[0].tv_nsec = times[0].tv_usec * 1000;
  ts_times[1].tv_sec  = times[1].tv_sec;
  ts_times[1].tv_nsec = times[1].tv_usec * 1000;

  return futimens(file, ts_times);
#else
  return futimes(file, times);
#endif
}

static File::Error CallFctnlFlock(PlatformFile file, bool do_lock) {
  struct flock lock;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;  // Lock entire file.
  if (HANDLE_EINTR(fcntl(file, do_lock ? F_SETLK : F_UNLCK, &lock)) == -1)
    return File::OSErrorToFileError(errno);
  return File::FILE_OK;
}
#else  // defined(OS_NACL)

static bool IsOpenAppend(PlatformFile file) {
  // NaCl doesn't implement fcntl. Since NaCl's write conforms to the POSIX
  // standard and always appends if the file is opened with O_APPEND, just
  // return false here.
  return false;
}

static int CallFtruncate(PlatformFile file, int64_t length) {
  NOTIMPLEMENTED();  // NaCl doesn't implement ftruncate.
  return 0;
}

static int CallFsync(PlatformFile file) {
  NOTIMPLEMENTED();  // NaCl doesn't implement fsync.
  return 0;
}

static int CallFutimes(PlatformFile file, const struct timeval times[2]) {
  NOTIMPLEMENTED();  // NaCl doesn't implement futimes.
  return 0;
}

static File::Error CallFctnlFlock(PlatformFile file, bool do_lock) {
  NOTIMPLEMENTED();  // NaCl doesn't implement flock struct.
  return File::FILE_ERROR_INVALID_OPERATION;
}
#endif  // defined(OS_NACL)

}  // namespace

void File::Info::FromStat(const stat_wrapper_t& stat_info) {
  // @tips
  // \file <sys/stat.h> 文件状态信息结构体
  is_directory = S_ISDIR(stat_info.st_mode);  // 是否是一个 directory
  is_symbolic_link = S_ISLNK(stat_info.st_mode);  // 是否是一个 symbolic link
  size = stat_info.st_size; // 文件大小

#if defined(OS_LINUX)
  time_t last_modified_sec = stat_info.st_mtim.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtim.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atim.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atim.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctim.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctim.tv_nsec;
#elif defined(OS_ANDROID)
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = stat_info.st_mtime_nsec;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = stat_info.st_atime_nsec;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = stat_info.st_ctime_nsec;
#elif defined(OS_MACOSX) || defined(OS_IOS) || defined(OS_BSD)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctimespec.tv_nsec;
#else
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = 0;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = 0;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = 0;
#endif

  last_modified =
      Time::FromTimeT(last_modified_sec) +
      TimeDelta::FromMicroseconds(last_modified_nsec /
                                  Time::kNanosecondsPerMicrosecond);

  last_accessed =
      Time::FromTimeT(last_accessed_sec) +
      TimeDelta::FromMicroseconds(last_accessed_nsec /
                                  Time::kNanosecondsPerMicrosecond);

  creation_time =
      Time::FromTimeT(creation_time_sec) +
      TimeDelta::FromMicroseconds(creation_time_nsec /
                                  Time::kNanosecondsPerMicrosecond);
}

// NaCl doesn't implement system calls to open files directly.
// 
// NaCl 不执行系统调用来直接打开文件。
#if !defined(OS_NACL)
// TODO(erikkay): does it make sense to support FLAG_EXCLUSIVE_* here?
// 
// 创建或打开给定的文件，允许具有遍历 （'..'） 组件的路径。 使用时要格外小心。
void File::InitializeUnsafe(const FilePath& name, uint32_t flags) {
  // I/O 线程检测
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!IsValid());

  // 创建文件标志，文件必须不存在，否则打开失败 O_EXCL 。
  int open_flags = 0;
  if (flags & FLAG_CREATE)
    open_flags = O_CREAT | O_EXCL;

  created_ = false;

  // 可能覆盖已有文件的内容方式打开文件（即，指定文件一定会被打开，并且内容为空），
  // 且必须有写入标志。
  if (flags & FLAG_CREATE_ALWAYS) {
    DCHECK(!open_flags);
    DCHECK(flags & FLAG_WRITE);
    open_flags = O_CREAT | O_TRUNC;
  }

  // 清除内容方式打开文件，且必须有写入标志。
  if (flags & FLAG_OPEN_TRUNCATED) {
    DCHECK(!open_flags);
    DCHECK(flags & FLAG_WRITE);
    open_flags = O_TRUNC;
  }

  // 至少有创建或打开文件的标志。
  if (!open_flags && !(flags & FLAG_OPEN) && !(flags & FLAG_OPEN_ALWAYS)) {
    NOTREACHED();
    errno = EOPNOTSUPP;
    error_details_ = FILE_ERROR_FAILED;
    return;
  }

  if (flags & FLAG_WRITE && flags & FLAG_READ) {
    // 读写标志
    open_flags |= O_RDWR;
  } else if (flags & FLAG_WRITE) {
    // 写入标志
    open_flags |= O_WRONLY;
  } else if (!(flags & FLAG_READ) &&
             !(flags & FLAG_WRITE_ATTRIBUTES) &&
             !(flags & FLAG_APPEND) &&
             !(flags & FLAG_OPEN_ALWAYS)) {
    NOTREACHED();
  }

  // 设备文件（串口）
  if (flags & FLAG_TERMINAL_DEVICE)
    open_flags |= O_NOCTTY | O_NDELAY;

  // 附加方式打开文件。必须是读写，或者只写方式。
  if (flags & FLAG_APPEND && flags & FLAG_READ)
    open_flags |= O_APPEND | O_RDWR;
  else if (flags & FLAG_APPEND)
    open_flags |= O_APPEND | O_WRONLY;

  COMPILE_ASSERT(O_RDONLY == 0, O_RDONLY_must_equal_zero);

  int mode = S_IRUSR | S_IWUSR;
#if defined(OS_CHROMEOS)
  mode |= S_IRGRP | S_IROTH;
#endif

  // 忽略 EINTR 信号中断，打开文件。
  int descriptor = HANDLE_EINTR(open(name.value().c_str(), open_flags, mode));

  if (flags & FLAG_OPEN_ALWAYS) {
    if (descriptor < 0) {
      // 打开失败（可能文件不存在），创建新文件。
      open_flags |= O_CREAT;
      // 独占访问标志。文件已存在，即创建失败。对已存在文件，独占访问，请考虑使用 Lock() 
      if (flags & FLAG_EXCLUSIVE_READ || flags & FLAG_EXCLUSIVE_WRITE)
        open_flags |= O_EXCL;   // together with O_CREAT implies O_NOFOLLOW

      descriptor = HANDLE_EINTR(open(name.value().c_str(), open_flags, mode));
      if (descriptor >= 0)
        created_ = true;
    }
  }

  // 打开失败，保存错误信息
  if (descriptor < 0) {
    error_details_ = File::OSErrorToFileError(errno);
    return;
  }

  if (flags & (FLAG_CREATE_ALWAYS | FLAG_CREATE))
    created_ = true;

  // @tips
  // 从文件系统中删除一个名称。如果名称是文件的最后一个连接，并且没有其它进程将文件打开，名称
  // 对应的文件会实际被删除。
  // 每一个文件，都可以通过 struct stat 的结构体来获得文件信息，其中一个成员 st_nlink 代表
  // 文件的链接数。
  // 当通过 shell 的 touch 命令或者在程序中 open 一个带有 O_CREAT 的不存在的文件时，文件
  // 的链接数为 1 。通常 open 一个已存在的文件不会影响文件的链接数。open 的作用只是使调用进
  // 程与文件之间建立一种访问关系，即 open 之后返回 fd ，调用进程可以通过 fd 来 read 、write 、
  // ftruncate 等等一系列对文件的操作。
  // close() 是消除这种调用进程与文件之间的访问关系。不会影响文件的链接数。在调用 close 时，
  // 内核会检查打开该文件的进程数，如果此数为 0 ，进一步检查文件的链接数，如果这个数也为 0 ，
  // 那么就删除文件。
  // link 函数是创建一个新目录项，并且增加一个链接数。 unlink 函数删除目录项，并且减少一个链
  // 接数。如果链接数达到 0 并且没有任何进程打开该文件，该文件内容才被真正删除。如果在 unlilnk 
  // 之前没有 close，那么依旧可以访问文件内容。（！！！注意）。
  // 
  // 综上所诉，真正影响链接数的操作是 link、 unlink 以及 open 的创建。
  // 删除文件内容的真正含义是文件的链接数为 0 ，而这个操作的本质完成者是 unlink 。close 能够
  // 实施删除文件内容的操作。同时，必定是因为在 close 之前有一个 unlink 操作才能删除。
  // 
  // 
  // 关闭文件时，随即删除。
  if (flags & FLAG_DELETE_ON_CLOSE)
    unlink(name.value().c_str());

  // 是否异步打开文件
  async_ = ((flags & FLAG_ASYNC) == FLAG_ASYNC);
  error_details_ = FILE_OK;
  file_.reset(descriptor);
}
#endif  // !defined(OS_NACL)

bool File::IsValid() const {
  return file_.is_valid();
}

PlatformFile File::GetPlatformFile() const {
  return file_.get();
}

// 释放当前文件文件描述符所有权，并返回之。
PlatformFile File::TakePlatformFile() {
  return file_.release();
}

void File::Close() {
  if (!IsValid())
    return;

  butil::ThreadRestrictions::AssertIOAllowed();
  file_.reset();
}

int64_t File::Seek(Whence whence, int64_t offset) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());

#if defined(OS_ANDROID)
  COMPILE_ASSERT(sizeof(int64_t) == sizeof(off64_t), off64_t_64_bit);
  return lseek64(file_.get(), static_cast<off64_t>(offset),
                 static_cast<int>(whence));
#else
  COMPILE_ASSERT(sizeof(int64_t) == sizeof(off_t), off_t_64_bit);
  return lseek(file_.get(), static_cast<off_t>(offset),
               static_cast<int>(whence));
#endif
}

int File::Read(int64_t offset, char* data, int size) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  // 读取指定长度文件内容（不适用流文件）。
  int bytes_read = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(pread(file_.get(), data + bytes_read,
                            size - bytes_read, offset + bytes_read));
    if (rv <= 0)
      break;

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : rv;
}

int File::ReadAtCurrentPos(char* data, int size) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  int bytes_read = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(read(file_.get(), data + bytes_read, size - bytes_read));
    if (rv <= 0)
      break;

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : rv;
}

int File::ReadNoBestEffort(int64_t offset, char* data, int size) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());

  return HANDLE_EINTR(pread(file_.get(), data, size, offset));
}

int File::ReadAtCurrentPosNoBestEffort(char* data, int size) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  return HANDLE_EINTR(read(file_.get(), data, size));
}

int File::Write(int64_t offset, const char* data, int size) {
  butil::ThreadRestrictions::AssertIOAllowed();

  // 是否使用 FLAG_APPEND 打开文件。
  if (IsOpenAppend(file_.get()))
    return WriteAtCurrentPos(data, size);

  DCHECK(IsValid());
  if (size < 0)
    return -1;

  int bytes_written = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(pwrite(file_.get(), data + bytes_written,
                             size - bytes_written, offset + bytes_written));
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : rv;
}

int File::WriteAtCurrentPos(const char* data, int size) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  int bytes_written = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(write(file_.get(), data + bytes_written,
                            size - bytes_written));
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : rv;
}

int File::WriteAtCurrentPosNoBestEffort(const char* data, int size) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  return HANDLE_EINTR(write(file_.get(), data, size));
}

int64_t File::GetLength() {
  DCHECK(IsValid());

  stat_wrapper_t file_info;
  if (CallFstat(file_.get(), &file_info))
    return false;

  return file_info.st_size;
}

bool File::SetLength(int64_t length) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());
  return !CallFtruncate(file_.get(), length);
}

bool File::Flush() {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());
  return !CallFsync(file_.get());
}

bool File::SetTimes(Time last_access_time, Time last_modified_time) {
  butil::ThreadRestrictions::AssertIOAllowed();
  DCHECK(IsValid());

  timeval times[2];
  times[0] = last_access_time.ToTimeVal();
  times[1] = last_modified_time.ToTimeVal();

  return !CallFutimes(file_.get(), times);
}

bool File::GetInfo(Info* info) {
  DCHECK(IsValid());

  stat_wrapper_t file_info;
  if (CallFstat(file_.get(), &file_info))
    return false;

  info->FromStat(file_info);
  return true;
}

File::Error File::Lock() {
  return CallFctnlFlock(file_.get(), true);
}

File::Error File::Unlock() {
  return CallFctnlFlock(file_.get(), false);
}

// Static.
File::Error File::OSErrorToFileError(int saved_errno) {
  switch (saved_errno) {
    case EACCES:
    case EISDIR:
    case EROFS:
    case EPERM:
      return FILE_ERROR_ACCESS_DENIED;
#if !defined(OS_NACL)  // ETXTBSY not defined by NaCl.
    case ETXTBSY:
      return FILE_ERROR_IN_USE;
#endif
    case EEXIST:
      return FILE_ERROR_EXISTS;
    case ENOENT:
      return FILE_ERROR_NOT_FOUND;
    case EMFILE:
      return FILE_ERROR_TOO_MANY_OPENED;
    case ENOMEM:
      return FILE_ERROR_NO_MEMORY;
    case ENOSPC:
      return FILE_ERROR_NO_SPACE;
    case ENOTDIR:
      return FILE_ERROR_NOT_A_DIRECTORY;
    default:
      return FILE_ERROR_FAILED;
  }
}

void File::SetPlatformFile(PlatformFile file) {
  DCHECK(!file_.is_valid());
  file_.reset(file);
}

}  // namespace butil

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_FILES_FILE_H_
#define BUTIL_FILES_FILE_H_

#include "butil/build_config.h"
#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_POSIX)
#include <sys/stat.h>
#endif

#include <string>

#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/files/scoped_file.h"
#include "butil/move.h"
#include "butil/time/time.h"

#if defined(OS_WIN)
#include "butil/win/scoped_handle.h"
#endif

namespace butil {

class FilePath;

// 文件句柄（ POSIX 文件描述符）
#if defined(OS_WIN)
typedef HANDLE PlatformFile;
#elif defined(OS_POSIX)
typedef int PlatformFile;

// 文件状态结构
#if defined(OS_BSD) || defined(OS_MACOSX) || defined(OS_NACL)
typedef struct stat stat_wrapper_t;
#else
typedef struct stat64 stat_wrapper_t;
#endif
#endif  // defined(OS_POSIX)

// Thin wrapper around an OS-level file.
// Note that this class does not provide any support for asynchronous IO, other
// than the ability to create asynchronous handles on Windows.
//
// Note about const: this class does not attempt to determine if the underlying
// file system object is affected by a particular method in order to consider
// that method const or not. Only methods that deal with member variables in an
// obvious non-modifying way are marked as const. Any method that forward calls
// to the OS is not considered const, even if there is no apparent change to
// member variables.
// 
// 操作系统级别文件操作包装器。
// 请注意，除了在 Windows 上创建异步句柄的能力外，此类不提供对异步 IO 的任何支持。
// 关于 const 的注意事项：这个类不会尝试确定底层文件系统对象是否受特定方法影响，以考虑该方法
// 是否为 const 。只有以显式的非修改方式处理成员变量的方法标记为 const 。即使对成员变量没有
// 明显改变，任何将调用转移到 OS 的方法都不被视为 const 。
class BUTIL_EXPORT File {
  // 仅支持移动拷贝操作
  MOVE_ONLY_TYPE_FOR_CPP_03(File, RValue)

 public:
  // FLAG_(OPEN|CREATE).* are mutually exclusive. You should specify exactly one
  // of the five (possibly combining with other flags) when opening or creating
  // a file.
  // FLAG_(WRITE|APPEND) are mutually exclusive. This is so that APPEND behavior
  // will be consistent with O_APPEND on POSIX.
  // FLAG_EXCLUSIVE_(READ|WRITE) only grant exclusive access to the file on
  // creation on POSIX; for existing files, consider using Lock().
  // 
  // FLAG_(OPEN|CREATE)* 是互斥的。应该在打开或创建文件时指定五个中的一个（可能与其他标志
  // 组合）。
  // FLAG_(WRITE|APPEND) 是互斥的。这样 APPEND 行为将与 POSIX 上的 O_APPEND 一致。
  // FLAG_EXCLUSIVE_(READ|WRITE) 只能对 POSIX 上创建的文件时进行独占校验；对于已有文件，
  // 请考虑使用 Lock() 。
  // 
  // 文件 flags 枚举类型。
  enum Flags {
    FLAG_OPEN = 1 << 0,             // Opens a file, only if it exists.
    FLAG_CREATE = 1 << 1,           // Creates a new file, only if it does not
                                    // already exist.
    FLAG_OPEN_ALWAYS = 1 << 2,      // May create a new file.
    FLAG_CREATE_ALWAYS = 1 << 3,    // May overwrite an old file.
    FLAG_OPEN_TRUNCATED = 1 << 4,   // Opens a file and truncates it, only if it
                                    // exists.
    FLAG_READ = 1 << 5,
    FLAG_WRITE = 1 << 6,
    FLAG_APPEND = 1 << 7,
    FLAG_EXCLUSIVE_READ = 1 << 8,   // EXCLUSIVE is opposite of Windows SHARE.
    FLAG_EXCLUSIVE_WRITE = 1 << 9,
    FLAG_ASYNC = 1 << 10,
    FLAG_TEMPORARY = 1 << 11,       // Used on Windows only.
    FLAG_HIDDEN = 1 << 12,          // Used on Windows only.
    FLAG_DELETE_ON_CLOSE = 1 << 13,
    FLAG_WRITE_ATTRIBUTES = 1 << 14,  // Used on Windows only.
    FLAG_SHARE_DELETE = 1 << 15,      // Used on Windows only.
    FLAG_TERMINAL_DEVICE = 1 << 16,   // Serial port flags.
    FLAG_BACKUP_SEMANTICS = 1 << 17,  // Used on Windows only.
    FLAG_EXECUTE = 1 << 18,           // Used on Windows only.
  };

  // This enum has been recorded in multiple histograms. If the order of the
  // fields needs to change, please ensure that those histograms are obsolete or
  // have been moved to a different enum.
  //
  // FILE_ERROR_ACCESS_DENIED is returned when a call fails because of a
  // filesystem restriction. FILE_ERROR_SECURITY is returned when a browser
  // policy doesn't allow the operation to be executed.
  // 
  // 当文件系统限制时，调用失败返回 FILE_ERROR_ACCESS_DENIED 。当浏览器策略不允许执行时，
  // 返回 FILE_ERROR_SECURITY 。
  enum Error {
    FILE_OK = 0,
    FILE_ERROR_FAILED = -1,
    FILE_ERROR_IN_USE = -2,
    FILE_ERROR_EXISTS = -3,
    FILE_ERROR_NOT_FOUND = -4,
    FILE_ERROR_ACCESS_DENIED = -5,
    FILE_ERROR_TOO_MANY_OPENED = -6,
    FILE_ERROR_NO_MEMORY = -7,
    FILE_ERROR_NO_SPACE = -8,
    FILE_ERROR_NOT_A_DIRECTORY = -9,
    FILE_ERROR_INVALID_OPERATION = -10,
    FILE_ERROR_SECURITY = -11,
    FILE_ERROR_ABORT = -12,
    FILE_ERROR_NOT_A_FILE = -13,
    FILE_ERROR_NOT_EMPTY = -14,
    FILE_ERROR_INVALID_URL = -15,
    FILE_ERROR_IO = -16,
    // Put new entries here and increment FILE_ERROR_MAX.
    FILE_ERROR_MAX = -17
  };

  // This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.
  // 
  // 必须与 Windows 上的 FILE_* 、 Linux 上的 SEEK_* 映射匹配。编译时，会静态断言。
  enum Whence {
    FROM_BEGIN   = 0,
    FROM_CURRENT = 1,
    FROM_END     = 2
  };

  // Used to hold information about a given file.
  // If you add more fields to this structure (platform-specific fields are OK),
  // make sure to update all functions that use it in file_util_{win|posix}.cc
  // too, and the ParamTraits<butil::PlatformFileInfo> implementation in
  // chrome/common/common_param_traits.cc.
  // 
  // 用于保存有关给定文件状态的信息。如果您向此结构添加更多字段（平台特定字段），请先确保
  // 在 file_util_{win|posix}.cc 中更新所有使用它的函数，及 chrome/common/common_param_traits.cc 
  // 的实现 ParamTraits<butil::PlatformFileInfo>
  struct BUTIL_EXPORT Info {
    Info();
    ~Info();
#if defined(OS_POSIX)
    // Fills this struct with values from |stat_info|.
    // 
    // 从 |stat_info| 填充文件状态信息到 Info 中
    void FromStat(const stat_wrapper_t& stat_info);
#endif

    // The size of the file in bytes.  Undefined when is_directory is true.
    // 
    // 文件大小。为目录时，未定义
    int64_t size;

    // True if the file corresponds to a directory.
    // 
    // 是否是目录
    bool is_directory;

    // True if the file corresponds to a symbolic link.
    // 
    // 是否是 symbolic link
    bool is_symbolic_link;

    // The last modified time of a file.
    // 
    // 最后修改
    butil::Time last_modified;

    // The last accessed time of a file.
    // 
    // 最后访问
    butil::Time last_accessed;

    // The creation time of a file.
    // 
    // 创建时间
    butil::Time creation_time;
  };

  File();

  // Creates or opens the given file. This will fail with 'access denied' if the
  // |name| contains path traversal ('..') components.
  // 
  // 创建或打开给定路径 |name| 文件。不能是引用父目录 ('..') ，否则返回 "access denied"
  File(const FilePath& name, uint32_t flags);

  // Takes ownership of |platform_file|.
  // 
  // 获取 |platform_file| 文件描述符所有权。
  explicit File(PlatformFile platform_file);

  // Creates an object with a specific error_details code.
  // 
  // 创建一个指定错误描述的 File 对象。
  explicit File(Error error_details);

  // Move constructor for C++03 move emulation of this type.
  // 
  // 移动拷贝构造函数。
  File(RValue other);

  ~File();

  // Move operator= for C++03 move emulation of this type.
  // 
  // 移动拷贝赋值函数。
  File& operator=(RValue other);

  // Creates or opens the given file.
  // 
  // 创建或打开给定文件 |name|
  void Initialize(const FilePath& name, uint32_t flags);

  // Creates or opens the given file, allowing paths with traversal ('..')
  // components. Use only with extreme care.
  // 
  // 创建或打开给定的文件，允许具有遍历 （'..'） 组件的路径。 使用时要格外小心。
  void InitializeUnsafe(const FilePath& name, uint32_t flags);

  bool IsValid() const;

  // Returns true if a new file was created (or an old one truncated to zero
  // length to simulate a new file, which can happen with
  // FLAG_CREATE_ALWAYS), and false otherwise.
  // 
  // 如果创建了新文件（或旧的文件被截断为零，可能会发生在 FLAG_CREATE_ALWAYS 中），则
  // 返回 true ，否则返回 false
  bool created() const { return created_; }

  // Returns the OS result of opening this file. Note that the way to verify
  // the success of the operation is to use IsValid(), not this method:
  //   File file(name, flags);
  //   if (!file.IsValid())
  //     return;
  Error error_details() const { return error_details_; }

  // 返回当前文件描述符
  PlatformFile GetPlatformFile() const;
  // 释放当前文件文件描述符所有权，并返回之。
  PlatformFile TakePlatformFile();

  // Destroying this object closes the file automatically.
  // 
  // 销毁对象会自动关闭文件。
  void Close();

  // Changes current position in the file to an |offset| relative to an origin
  // defined by |whence|. Returns the resultant current position in the file
  // (relative to the start) or -1 in case of error.
  // 
  // 相对于由 |whence| 定义的原点，将文件中的当前位置更改为 |offset| 。返回文件当前位置
  // （相对于开始）或 -1 ，以防出错。
  int64_t Seek(Whence whence, int64_t offset);

  // Reads the given number of bytes (or until EOF is reached) starting with the
  // given offset. Returns the number of bytes read, or -1 on error. Note that
  // this function makes a best effort to read all data on all platforms, so it
  // is not intended for stream oriented files but instead for cases when the
  // normal expectation is that actually |size| bytes are read unless there is
  // an error.
  // 
  // 从给定的偏移量开始读取给定的字节数（或直到达到 EOF ）。返回读取的字节数，错误时为 -1。
  // 请注意，此函数尽最大努力读取所有平台上的所有数据，因此它不适用于面向流的文件，而是适用
  // 于读取正常期望值为 size 实际大小文件，除非有错误。
  int Read(int64_t offset, char* data, int size);

  // Same as above but without seek.
  // 
  // Read 读取当前偏移量的文件内容版本
  int ReadAtCurrentPos(char* data, int size);

  // Reads the given number of bytes (or until EOF is reached) starting with the
  // given offset, but does not make any effort to read all data on all
  // platforms. Returns the number of bytes read, or -1 on error.
  // 
  // 从给定的偏移量开始读取给定数量的字节（或直到达到 EOF ），但不会花费任何精力读取所有平台
  // 上的所有数据。返回读取的字节数，错误时为 -1
  int ReadNoBestEffort(int64_t offset, char* data, int size);

  // Same as above but without seek.
  // 
  // ReadNoBestEffort 读取当前偏移量的文件内容的版本
  int ReadAtCurrentPosNoBestEffort(char* data, int size);

  // Writes the given buffer into the file at the given offset, overwritting any
  // data that was previously there. Returns the number of bytes written, or -1
  // on error. Note that this function makes a best effort to write all data on
  // all platforms.
  // Ignores the offset and writes to the end of the file if the file was opened
  // with FLAG_APPEND.
  // 
  // 将给定缓冲区写入给定偏移量的文件中，覆盖之前存在的所有数据。返回写入字节数，错误时返回 -1 
  // 请注意，此功能尽最大努力在所有平台上编写所有数据。如果使用 FLAG_APPEND 打开文件，则忽略
  // 偏移量并写入文件末尾。
  int Write(int64_t offset, const char* data, int size);

  // Save as above but without seek.
  // 
  // Write 写入当前偏移量的文件内容版本
  int WriteAtCurrentPos(const char* data, int size);

  // Save as above but does not make any effort to write all data on all
  // platforms. Returns the number of bytes written, or -1 on error.
  // 
  // 相对 Write ，不会在所有平台上写入所有数据。返回写入的字节数，错误时返回 -1
  int WriteAtCurrentPosNoBestEffort(const char* data, int size);

  // Returns the current size of this file, or a negative number on failure.
  // 
  // 返回此文件的当前大小或失败时的负数。
  int64_t GetLength();

  // Truncates the file to the given length. If |length| is greater than the
  // current size of the file, the file is extended with zeros. If the file
  // doesn't exist, |false| is returned.
  // 
  // 将文件截断为给定的长度。如果 |length| 大于文件的当前大小，文件被扩展为零（"空洞"）。
  // 如果文件不存在，|false| 返回。
  bool SetLength(int64_t length);

  // Flushes the buffers.
  // 
  // 刷新文件缓冲区。 fsync(fd);
  bool Flush();

  // Updates the file times.
  // 
  // 更新文件时间（访问时间、最后更新时间）
  bool SetTimes(Time last_access_time, Time last_modified_time);

  // Returns some basic information for the given file.
  // 
  // 将文件信息写入 |info| 中
  bool GetInfo(Info* info);

  // Attempts to take an exclusive write lock on the file. Returns immediately
  // (i.e. does not wait for another process to unlock the file). If the lock
  // was obtained, the result will be FILE_OK. A lock only guarantees
  // that other processes may not also take a lock on the same file with the
  // same API - it may still be opened, renamed, unlinked, etc.
  //
  // Common semantics:
  //  * Locks are held by processes, but not inherited by child processes.
  //  * Locks are released by the OS on file close or process termination.
  //  * Locks are reliable only on local filesystems.
  //  * Duplicated file handles may also write to locked files.
  // Windows-specific semantics:
  //  * Locks are mandatory for read/write APIs, advisory for mapping APIs.
  //  * Within a process, locking the same file (by the same or new handle)
  //    will fail.
  // POSIX-specific semantics:
  //  * Locks are advisory only.
  //  * Within a process, locking the same file (by the same or new handle)
  //    will succeed.
  //  * Closing any descriptor on a given file releases the lock.
  //  
  // 
  // 尝试对文件进行独占写入锁定，立即返回（即不等待另一个进程解锁文件）。如果获得锁，结果将
  // 是 FILE_OK 。一个锁只能保证其他进程不会对同一个文件使用相同的 API 进行锁定 - 它仍然
  // 可以被 opened, renamed, unlinked 等。
  // 
  // 通用语义：
  // *锁由进程持有，但不由子进程继承。
  // *文件关闭或进程终止时，操作系统释放锁。
  // *锁只在本地文件系统上可靠。
  // *重复的文件句柄也可能写入锁定的文件。
  // 
  // 特定于 Windows 的语义：
  // *读/写 API 必须使用锁，针对映射 API 的建议。在一个进程中，锁定相同的文件（通过相同或新的
  // 句柄）将会失败。
  // 
  // POSIX 特定的语义：
  // *锁只是建议性的。
  // *在一个进程中，锁定相同的文件（通过相同或新的句柄）将成功。
  // *关闭给定文件上的任何描述符将释放该锁。
  Error Lock();

  // Unlock a file previously locked.
  // 
  // 解锁先前锁定的文件。
  Error Unlock();

  bool async() const { return async_; }

#if defined(OS_WIN)
  static Error OSErrorToFileError(DWORD last_error);
#elif defined(OS_POSIX)
  static Error OSErrorToFileError(int saved_errno);
#endif

  // Converts an error value to a human-readable form. Used for logging.
  // 
  // 将错误值转换为可读的形式。用于日志记录。
  static std::string ErrorToString(Error error);

 private:
  void SetPlatformFile(PlatformFile file);

#if defined(OS_WIN)
  win::ScopedHandle file_;
#elif defined(OS_POSIX)
  ScopedFD file_;   // 文件句柄
#endif

  Error error_details_;
  bool created_;
  bool async_;
};

}  // namespace butil

#endif  // BUTIL_FILES_FILE_H_

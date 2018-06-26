// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FilePath 对象旨在用于任何路径（兼容不同系统）。应用程序可以在内部传递 FilePath 对象，
// 从而掩盖系统之间的潜在差异，仅在直接使用面向系统接口的实现时才有所不同。

// FilePath is a container for pathnames stored in a platform's native string
// type, providing containers for manipulation in according with the
// platform's conventions for pathnames.  It supports the following path
// types:
//
//                   POSIX            Windows
//                   ---------------  ----------------------------------
// Fundamental type  char[]           wchar_t[]
// Encoding          unspecified*     UTF-16
// Separator         /                \, tolerant of /
// Drive letters     no               case-insensitive A-Z followed by :
// Alternate root    // (surprise!)   \\, for UNC paths
//
// * The encoding need not be specified on POSIX systems, although some
//   POSIX-compliant systems do specify an encoding.  Mac OS X uses UTF-8.
//   Chrome OS also uses UTF-8.
//   Linux does not specify an encoding, but in practice, the locale's
//   character set may be used.
//
// For more arcane bits of path trivia, see below.
//
// FilePath objects are intended to be used anywhere paths are.  An
// application may pass FilePath objects around internally, masking the
// underlying differences between systems, only differing in implementation
// where interfacing directly with the system.  For example, a single
// OpenFile(const FilePath &) function may be made available, allowing all
// callers to operate without regard to the underlying implementation.  On
// POSIX-like platforms, OpenFile might wrap fopen, and on Windows, it might
// wrap _wfopen_s, perhaps both by calling file_path.value().c_str().  This
// allows each platform to pass pathnames around without requiring conversions
// between encodings, which has an impact on performance, but more imporantly,
// has an impact on correctness on platforms that do not have well-defined
// encodings for pathnames.
//
// Several methods are available to perform common operations on a FilePath
// object, such as determining the parent directory (DirName), isolating the
// final path component (BaseName), and appending a relative pathname string
// to an existing FilePath object (Append).  These methods are highly
// recommended over attempting to split and concatenate strings directly.
// These methods are based purely on string manipulation and knowledge of
// platform-specific pathname conventions, and do not consult the filesystem
// at all, making them safe to use without fear of blocking on I/O operations.
// These methods do not function as mutators but instead return distinct
// instances of FilePath objects, and are therefore safe to use on const
// objects.  The objects themselves are safe to share between threads.
//
// To aid in initialization of FilePath objects from string literals, a
// FILE_PATH_LITERAL macro is provided, which accounts for the difference
// between char[]-based pathnames on POSIX systems and wchar_t[]-based
// pathnames on Windows.
//
// Paths can't contain NULs as a precaution agaist premature truncation.
//
// Because a FilePath object should not be instantiated at the global scope,
// instead, use a FilePath::CharType[] and initialize it with
// FILE_PATH_LITERAL.  At runtime, a FilePath object can be created from the
// character array.  Example:
//
// | const FilePath::CharType kLogFileName[] = FILE_PATH_LITERAL("log.txt");
// |
// | void Function() {
// |   FilePath log_file_path(kLogFileName);
// |   [...]
// | }
//
// WARNING: FilePaths should ALWAYS be displayed with LTR directionality, even
// when the UI language is RTL. This means you always need to pass filepaths
// through butil::i18n::WrapPathWithLTRFormatting() before displaying it in the
// RTL UI.
//
// This is a very common source of bugs, please try to keep this in mind.
//
// ARCANE BITS OF PATH TRIVIA
//
//  - A double leading slash is actually part of the POSIX standard.  Systems
//    are allowed to treat // as an alternate root, as Windows does for UNC
//    (network share) paths.  Most POSIX systems don't do anything special
//    with two leading slashes, but FilePath handles this case properly
//    in case it ever comes across such a system.  FilePath needs this support
//    for Windows UNC paths, anyway.
//    References:
//    The Open Group Base Specifications Issue 7, sections 3.266 ("Pathname")
//    and 4.12 ("Pathname Resolution"), available at:
//    http://www.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_266
//    http://www.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_12
//
//  - Windows treats c:\\ the same way it treats \\.  This was intended to
//    allow older applications that require drive letters to support UNC paths
//    like \\server\share\path, by permitting c:\\server\share\path as an
//    equivalent.  Since the OS treats these paths specially, FilePath needs
//    to do the same.  Since Windows can use either / or \ as the separator,
//    FilePath treats c://, c:\\, //, and \\ all equivalently.
//    Reference:
//    The Old New Thing, "Why is a drive letter permitted in front of UNC
//    paths (sometimes)?", available at:
//    http://blogs.msdn.com/oldnewthing/archive/2005/11/22/495740.aspx
//    
//    
// FilePath 是一个用于存储平台相关本机字符串类型 (POSIX:char[], Windows:wchar_t) 的路径名的
// 容器，它是一个提供了根据平台路径名约定规则进行操作的容器。
//                   POSIX            Windows
//                   ---------------  ----------------------------------
// Fundamental type  char[]           wchar_t[]
// Encoding          unspecified*     UTF-16
// Separator         /                \, tolerant of /
// Drive letters     no               case-insensitive A-Z followed by :
// Alternate root    // (surprise!)   \\, for UNC paths
//
// 不需要在 POSIX 系统上指定编码，尽管一些符合 POSIX 的系统确实指定了编码。Mac OS X 使用 UTF-8。
// Chrome 操作系统也使用 UTF-8 。 Linux 不指定编码，但实际上可以使用语言环境的字符集。
// 
// FilePath 对象旨在用于任何路径（兼容不同系统）。应用程序可以在内部传递 FilePath 对象，从而掩盖
// 系统之间的潜在差异，仅在直接使用面向系统接口的实现时才有所不同。例如，OpenFile(const FilePath &) 
// 函数，允许所有调用者在不考虑底层实现的情况下执行。在类似 POSIX 的平台上， OpenFile 可能会封装 
// fopen ，而在 Windows 上，它可能会封装 _wfopen_s ，也许两者都要通过调用 file_path.value().
// c_str() 来实现。这样，每个平台都可以传递路径名，而不需要编码再进行转换。但更为重要的是，不要明确
// 定义路径编码，也能在不同平台工作的很好。
//
// 在 FilePath 对象有几个常见通用操作方法，例如，获取父目录 (DirName)，获取最终路径 (BaseName) ，
// 并将相对路径名字符串附加到现有 FilePath 对象 (Append) 上。强烈推荐使用这些方法，他们可直接尝试
// 分割和连接字符串。这些方法完全基于字符串操作和平台特定路径名约定的知识，可以安全的使用，并且根本不
// 查阅文件系统，也不用担心在 I/O 操作上阻塞。这些方法没有 mutator 的属性，而是返回 FilePath 对象
// 的不同实例，因此可以安全地在 const 对象上使用。对象本身可以安全地在线程之间共享。
// 
// 并且路径不使用 NULL 来防止过早截断的字符的预防措施。
//
// 不应该在全局域中实例化 FilePath 对象，而应使用 FilePath::CharType[] 并用 FILE_PATH_LITERAL 
// 进行初始化。然后在运行期，可以再从该字符数组创建一个 FilePath 对象。
// 
// Example:
// | const FilePath::CharType kLogFileName[] = FILE_PATH_LITERAL("log.txt");
// |
// | void Function() {
// |   FilePath log_file_path(kLogFileName);
// |   [...]
// | }
//

#ifndef BUTIL_FILES_FILE_PATH_H_
#define BUTIL_FILES_FILE_PATH_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "butil/base_export.h"
#include "butil/compiler_specific.h"
#include "butil/containers/hash_tables.h"
#include "butil/strings/string16.h"
#include "butil/strings/string_piece.h"  // For implicit conversions.
#include "butil/build_config.h"

// Windows-style drive letter support and pathname separator characters can be
// enabled and disabled independently, to aid testing.  These #defines are
// here so that the same setting can be used in both the implementation and
// in the unit test.
// 
// 可以单独启用和禁用 Windows 风格的驱动器号支持和路径名分隔符，以帮助测试。
#if defined(OS_WIN)
// 驱动器、分隔符
#define FILE_PATH_USES_DRIVE_LETTERS
#define FILE_PATH_USES_WIN_SEPARATORS
#endif  // OS_WIN

namespace butil {

// An abstraction to isolate users from the differences between native
// pathnames on different platforms.
// 
// 抽象不同平台本机路径名类。
class BUTIL_EXPORT FilePath {
 public:
#if defined(OS_POSIX)
  // On most platforms, native pathnames are char arrays, and the encoding
  // may or may not be specified.  On Mac OS X, native pathnames are encoded
  // in UTF-8.
  // 
  // 在大多数平台上，本机路径名是 char 数组，可能指定或不指定编码。在 Mac OS X 上，
  // 本机路径名以 UTF-8 编码。
  typedef std::string StringType;
#elif defined(OS_WIN)
  // On Windows, for Unicode-aware applications, native pathnames are wchar_t
  // arrays encoded in UTF-16.
  // 
  // 在 Windows 上，对于支持 Unicode 的应用程序，本机路径名是以 UTF-16 编码的 wchar_t 
  // 数组。
  typedef std::wstring StringType;
#endif  // OS_WIN

  // 路径底层字符的类型： CharType 类型在 POSIX 上为 char ，在 Windows 上为 wchar_t
  typedef StringType::value_type CharType;

  // Null-terminated array of separators used to separate components in
  // hierarchical paths.  Each character in this array is a valid separator,
  // but kSeparators[0] is treated as the canonical separator and will be used
  // when composing pathnames.
  // 
  // 以 NULL 为结尾的，用于分隔分层路径的分隔符字符串。该数组中的每个字符都是一个有效的
  // 分隔符，但是 kSeparators[0] 被视为规范的分隔符，当组成路径名时将被使用。
  static const CharType kSeparators[];

  // arraysize(kSeparators).
  // 
  // 分隔符长度
  static const size_t kSeparatorsLength;

  // A special path component meaning "this directory."
  // 
  // 当前路径特殊符 ('.')
  static const CharType kCurrentDirectory[];

  // A special path component meaning "the parent directory."
  // 
  // 父目录路径特殊符 ('..')
  static const CharType kParentDirectory[];

  // The character used to identify a file extension.
  // 
  // 文件扩展名分隔符 ('.')
  static const CharType kExtensionSeparator;

  FilePath();
  FilePath(const FilePath& that);
  explicit FilePath(const StringType& path);
  ~FilePath();
  FilePath& operator=(const FilePath& that);

  bool operator==(const FilePath& that) const;

  bool operator!=(const FilePath& that) const;

  // Required for some STL containers and operations
  // 
  // 标准容器 stl(map,unordered_map) 的比较器
  bool operator<(const FilePath& that) const {
    return path_ < that.path_;
  }

  // 路径字符串
  const StringType& value() const { return path_; }

  // 路径是否为空
  bool empty() const { return path_.empty(); }

  // 清除路径名
  void clear() { path_.clear(); }

  // Returns true if |character| is in kSeparators.
  // 
  // 返回字符 |character| 是否为路径分隔符
  static bool IsSeparator(CharType character);

  // Returns a vector of all of the components of the provided path. It is
  // equivalent to calling DirName().value() on the path's root component,
  // and BaseName().value() on each child component.
  //
  // To make sure this is lossless so we can differentiate absolute and
  // relative paths, the root slash will be included even though no other
  // slashes will be. The precise behavior is:
  //
  // Posix:  "/foo/bar"  ->  [ "/", "foo", "bar" ]
  // Windows:  "C:\foo\bar"  ->  [ "C:", "\\", "foo", "bar" ]
  // 
  // 
  // 返回当前路径的所有组件（目录）的数组。它相当于在路径的根组件上调用 DirName().
  // value() ，并在每个子组件上调用 BaseName().value() 。为了确保这是无损的，并
  // 且为了区分绝对和相对路径，即使没有其他斜线也将会包括根斜线。
  void GetComponents(std::vector<FilePath::StringType>* components) const;

  // Returns true if this FilePath is a strict parent of the |child|. Absolute
  // and relative paths are accepted i.e. is /foo parent to /foo/bar and
  // is foo parent to foo/bar. Does not convert paths to absolute, follow
  // symlinks or directory navigation (e.g. ".."). A path is *NOT* its own
  // parent.
  // 
  // 如果此 FilePath 是 |child| 的严格父目录，则返回 true 。接受绝对和相对路径，
  // 即 /foo 是 /foo/bar 的父目录，并且 foo 是 foo/bar 的父目录。不转换路径为绝
  // 对路径，遵循符号链接或目录导航 (e.g. "..") 。
  bool IsParent(const FilePath& child) const;

  // If IsParent(child) holds, appends to path (if non-NULL) the
  // relative path to child and returns true.  For example, if parent
  // holds "/Users/johndoe/Library/Application Support", child holds
  // "/Users/johndoe/Library/Application Support/Google/Chrome/Default", and
  // *path holds "/Users/johndoe/Library/Caches", then after
  // parent.AppendRelativePath(child, path) is called *path will hold
  // "/Users/johndoe/Library/Caches/Google/Chrome/Default".  Otherwise,
  // returns false.
  // 
  // 将 |child| 相对当前路径部分追加到 path（如果不为空）并返回 true 
  // 
  // Use like:
  // 父级是 "/Users/johndoe/Library/Application Support" ，子目录是 "/Users/
  // johndoe/Library/Application Support/Google/Chrome/Default" ，|*path| 是
  //  "/Users/johndoe/Library/Caches" ，然后在 parent.AppendRelativePath(child, path) 
  // 被调用后， |*path| 将会是 "/Users/johndoe/Library/Caches/Google/Chrome/Default" 
  // 否则返回 false
  bool AppendRelativePath(const FilePath& child, FilePath* path) const;

  // Returns a FilePath corresponding to the directory containing the path
  // named by this object, stripping away the file component.  If this object
  // only contains one component, returns a FilePath identifying
  // kCurrentDirectory.  If this object already refers to the root directory,
  // returns a FilePath identifying the root directory.
  // 
  // 返回本对象 path 路径中不包括文件名的目录包装的 FilePath 。如果此对象仅包含一个组件，
  // 则返回一个标识 kCurrentDirectory 的 FilePath 。如果此对象已经引用根目录，则返回一
  // 个标识根目录的 FilePath 。
  FilePath DirName() const WARN_UNUSED_RESULT;

  // Returns a FilePath corresponding to the last path component of this
  // object, either a file or a directory.  If this object already refers to
  // the root directory, returns a FilePath identifying the root directory;
  // this is the only situation in which BaseName will return an absolute path.
  // 
  // 返回此对象的最后一个路径组件（文件或目录）相对应的 FilePath 。如果此对象已经引用根目
  // 录，则返回一个标识根目录的 FilePath ，这是 BaseName 将返回绝对路径的唯一情况。
  FilePath BaseName() const WARN_UNUSED_RESULT;

  // Returns ".jpg" for path "C:\pics\jojo.jpg", or an empty string if
  // the file has no extension.  If non-empty, Extension() will always start
  // with precisely one ".".  The following code should always work regardless
  // of the value of path.  For common double-extensions like .tar.gz and
  // .user.js, this method returns the combined extension.  For a single
  // component, use FinalExtension().
  // new_path = path.RemoveExtension().value().append(path.Extension());
  // ASSERT(new_path == path.value());
  // NOTE: this is different from the original file_util implementation which
  // returned the extension without a leading "." ("jpg" instead of ".jpg")
  // 
  // 路径 "C:\pics\jojo.jpg" 返回 ".jpg" ，如果文件没有扩展名，则返回空字符串。如果不为
  // 空，则 Extension() 将始终以一个 "." 开头。对于常见的双扩展（如.tar.gz和.user.js），
  // 此方法返回组合扩展名， FinalExtension() 只返回一个。
  // 
  // 无论路径的值如何，以下代码应始终工作：
  // 
  // new_path = path.RemoveExtension().value().append(path.Extension());
  // ASSERT(new_path == path.value());
  // 
  // 注意：这与原始的 file_util 只返回没有前到 "." 实现不同 ("jpg"而不是 ".jpg")
  StringType Extension() const;

  // Returns the path's file extension, as in Extension(), but will
  // never return a double extension.
  //
  // TODO(davidben): Check all our extension-sensitive code to see if
  // we can rename this to Extension() and the other to something like
  // LongExtension(), defaulting to short extensions and leaving the
  // long "extensions" to logic like butil::GetUniquePathNumber().
  // 
  // 始终返回最后一个扩展名。
  StringType FinalExtension() const;

  // Returns "C:\pics\jojo" for path "C:\pics\jojo.jpg"
  // NOTE: this is slightly different from the similar file_util implementation
  // which returned simply 'jojo'.
  // 
  // 去除扩展名，如 "C:\pics\jojo.jpg" 转换为 "C:\pics\jojo"
  FilePath RemoveExtension() const WARN_UNUSED_RESULT;

  // Removes the path's file extension, as in RemoveExtension(), but
  // ignores double extensions.
  // 
  // 始终去除最后一个扩展名。
  FilePath RemoveFinalExtension() const WARN_UNUSED_RESULT;

  // Inserts |suffix| after the file name portion of |path| but before the
  // extension.  Returns "" if BaseName() == "." or "..".
  // Examples:
  // path == "C:\pics\jojo.jpg" suffix == " (1)", returns "C:\pics\jojo (1).jpg"
  // path == "jojo.jpg"         suffix == " (1)", returns "jojo (1).jpg"
  // path == "C:\pics\jojo"     suffix == " (1)", returns "C:\pics\jojo (1)"
  // path == "C:\pics.old\jojo" suffix == " (1)", returns "C:\pics.old\jojo (1)"
  // 
  // 在文件名后，扩展名前插入 |suffix| 字符串。如果 BaseName() == "." || ".." ，返回空。
  FilePath InsertBeforeExtension(
      const StringType& suffix) const WARN_UNUSED_RESULT;
  FilePath InsertBeforeExtensionASCII(
      const butil::StringPiece& suffix) const WARN_UNUSED_RESULT;

  // Adds |extension| to |file_name|. Returns the current FilePath if
  // |extension| is empty. Returns "" if BaseName() == "." or "..".
  // 
  // 在路径后添加扩展名。若 |extension| 为空，返回当前路径。如果 BaseName() == "." 
  // || ".." ，返回空。
  FilePath AddExtension(
      const StringType& extension) const WARN_UNUSED_RESULT;

  // Replaces the extension of |file_name| with |extension|.  If |file_name|
  // does not have an extension, then |extension| is added.  If |extension| is
  // empty, then the extension is removed from |file_name|.
  // Returns "" if BaseName() == "." or "..".
  // 
  // 用 |extension| 替换 |file_name| 的扩展名；如果 |file_name| 没有扩展名，则添加
  // 扩展名；如果 |extension| 为空，则移除 |file_name| 中扩展名。如果 BaseName() == 
  // "." || ".." ，返回空。
  FilePath ReplaceExtension(
      const StringType& extension) const WARN_UNUSED_RESULT;

  // Returns true if the file path matches the specified extension. The test is
  // case insensitive. Don't forget the leading period if appropriate.
  // 
  // 不区分大小写匹配文件路径的扩展名 |extension| ，匹配返回 true
  bool MatchesExtension(const StringType& extension) const;

  // Returns a FilePath by appending a separator and the supplied path
  // component to this object's path.  Append takes care to avoid adding
  // excessive separators if this object's path already ends with a separator.
  // If this object's path is kCurrentDirectory, a new FilePath corresponding
  // only to |component| is returned.  |component| must be a relative path;
  // it is an error to pass an absolute path.
  // 
  // 通过提供的路径组件 |component| 附加到当前对象的路径来返回新的 FilePath 。如果当前
  // 对象的路径已经以分隔符结束，那么 Append 注意避免添加过多的分隔符。如果此对象的路径是 
  // kCurrentDirectory ，则新的 FilePath 仅对应于 |component| 返回。 |component| 
  // 必须是相对路径;传递绝对路径是错误的。
  // 
  // 一个典型应用：当前路径为根目录， |component| 为根目录下的一个文件，调用该函数返回一
  // 个全路径。
  FilePath Append(const StringType& component) const WARN_UNUSED_RESULT;
  FilePath Append(const FilePath& component) const WARN_UNUSED_RESULT;

  // Although Windows StringType is std::wstring, since the encoding it uses for
  // paths is well defined, it can handle ASCII path components as well.
  // Mac uses UTF8, and since ASCII is a subset of that, it works there as well.
  // On Linux, although it can use any 8-bit encoding for paths, we assume that
  // ASCII is a valid subset, regardless of the encoding, since many operating
  // system paths will always be ASCII.
  // 
  // 尽管 Windows StringType 是 std::wstring ，但由于它用于路径的编码已经定义好的，它
  // 也可以处理 ASCII 路径组件。 Mac 使用 UTF8 ，并且由于 ASCII 是其中的一个子集，因此
  // 它也可以使用。在 Linux 上，虽然它可以使用任何 8 位编码路径，但我们认为 ASCII 是一个
  // 有效的子集，因为无论编码如何，许多操作系统路径始终是 ASCII 。
  FilePath AppendASCII(const butil::StringPiece& component)
      const WARN_UNUSED_RESULT;

  // Returns true if this FilePath contains an absolute path.  On Windows, an
  // absolute path begins with either a drive letter specification followed by
  // a separator character, or with two separator characters.  On POSIX
  // platforms, an absolute path begins with a separator character.
  // 
  // 如果此 FilePath 包含绝对路径，则返回 true 。在 Windows 上，绝对路径是以驱动器字母
  // 规范后跟分隔符或两个分隔符字符开头。在 POSIX 平台上，绝对路径以分隔符开头。
  bool IsAbsolute() const;

  // Returns true if the patch ends with a path separator character.
  // 
  // 如果此 FilePath 以路径分隔符结尾，则返回 true
  bool EndsWithSeparator() const WARN_UNUSED_RESULT;

  // Returns a copy of this FilePath that ends with a trailing separator. If
  // the input path is empty, an empty FilePath will be returned.
  // 
  // 返回以分隔符结尾的 FilePath 的副本。如果输入路径为空，则返回一个空的 FilePath
  FilePath AsEndingWithSeparator() const WARN_UNUSED_RESULT;

  // Returns a copy of this FilePath that does not end with a trailing
  // separator.
  // 
  // 返回不以分隔符结束的 FilePath 的副本。
  FilePath StripTrailingSeparators() const WARN_UNUSED_RESULT;

  // Returns true if this FilePath contains an attempt to reference a parent
  // directory (e.g. has a path component that is "..").
  // 
  // 如果 FilePath 包含尝试引用父目录（例如，具有 ".." 的路径组件），则返回 true
  bool ReferencesParent() const;

  // Return a Unicode human-readable version of this path.
  // Warning: you can *not*, in general, go from a display name back to a real
  // path.  Only use this when displaying paths to users, not just when you
  // want to stuff a string16 into some other API.
  // 
  // 返回此路径的 Unicode 可读版本。警告：一般而言，您可以（*不可以*）从显示名称的返回到真
  // 实路径。只有在向用户显示路径时才使用它，而不仅仅是当你想将 string16 放入其他 API 时。
  string16 LossyDisplayName() const;

  // Return the path as ASCII, or the empty string if the path is not ASCII.
  // This should only be used for cases where the FilePath is representing a
  // known-ASCII filename.
  // 
  // 以 ASCII 格式返回路径，如果路径不是 ASCII ，则返回空字符串。这应该仅用于 FilePath 表
  // 示已知 ASCII 文件名的情况。
  std::string MaybeAsASCII() const;

  // Return the path as UTF-8.
  //
  // This function is *unsafe* as there is no way to tell what encoding is
  // used in file names on POSIX systems other than Mac and Chrome OS,
  // although UTF-8 is practically used everywhere these days. To mitigate
  // the encoding issue, this function internally calls
  // SysNativeMBToWide() on POSIX systems other than Mac and Chrome OS,
  // per assumption that the current locale's encoding is used in file
  // names, but this isn't a perfect solution.
  //
  // Once it becomes safe to to stop caring about non-UTF-8 file names,
  // the SysNativeMBToWide() hack will be removed from the code, along
  // with "Unsafe" in the function name.
  // 
  // 以 UTF-8 的形式返回路径。
  // 
  // 这个函数是 *unsafe* 的，因为在 Mac 和 Chrome OS 以外的 POSIX 系统上，没有办法
  // 知道在文件名中使用了什么编码，但是现在在任何地方都可以使用 UTF-8 。为了缓解编码问
  // 题，根据假定当前语言环境的编码用于文件名中，此函数在 Mac 和 Chrome OS 以外的 POSIX 
  // 系统上内部调用 SysNativeMBToWide() ， 但这不是一个完美的解决方案。
  // 
  // 一旦停止关注非 UTF-8 文件名变得安全， SysNativeMBToWide() hack 将从代码中删除，
  // 以及函数名称中的 "Unsafe"
  std::string AsUTF8Unsafe() const;

  // Similar to AsUTF8Unsafe, but returns UTF-16 instead.
  // 
  // 以 UTF-16 的形式返回路径。
  string16 AsUTF16Unsafe() const;

  // Returns a FilePath object from a path name in UTF-8. This function
  // should only be used for cases where you are sure that the input
  // string is UTF-8.
  //
  // Like AsUTF8Unsafe(), this function is unsafe. This function
  // internally calls SysWideToNativeMB() on POSIX systems other than Mac
  // and Chrome OS, to mitigate the encoding issue. See the comment at
  // AsUTF8Unsafe() for details.
  // 
  // 以 UTF-8 的路径名称返回 FilePath 对象。这个函数只能用于你确定输入字符串是 UTF-8 
  // 的情况。
  // 
  // 像 AsUTF8Unsafe() 一样，这个函数是不安全的。此功能在 Mac 和 Chrome OS 以外的 POSIX 
  // 系统上内部调用 SysWideToNativeMB() ，以缓解编码问题。有关详细信息，请参阅 AsUTF8Unsafe() 
  // 上的注释。
  static FilePath FromUTF8Unsafe(const std::string& utf8);

  // Similar to FromUTF8Unsafe, but accepts UTF-16 instead.
  // 
  // 以 UTF-16 的路径名称返回 FilePath 对象。
  static FilePath FromUTF16Unsafe(const string16& utf16);

  // Normalize all path separators to backslash on Windows
  // (if FILE_PATH_USES_WIN_SEPARATORS is true), or do nothing on POSIX systems.
  // 
  // 在 Windows 上将所有路径分隔符标准化为反斜杠（如果 FILE_PATH_USES_WIN_SEPARATORS 为 
  // true ），或者在 POSIX 系统上不执行任何操作。
  FilePath NormalizePathSeparators() const;

  // Normalize all path separattors to given type on Windows
  // (if FILE_PATH_USES_WIN_SEPARATORS is true), or do nothing on POSIX systems.
  // 
  // 在 Windows 上将所有路径分隔符标准化为给定类型（如果 FILE_PATH_USES_WIN_SEPARATORS 为 
  // true ），或者在 POSIX 系统上不做任何事情。
  FilePath NormalizePathSeparatorsTo(CharType separator) const;

  // Compare two strings in the same way the file system does.
  // Note that these always ignore case, even on file systems that are case-
  // sensitive. If case-sensitive comparison is ever needed, add corresponding
  // methods here.
  // The methods are written as a static method so that they can also be used
  // on parts of a file path, e.g., just the extension.
  // CompareIgnoreCase() returns -1, 0 or 1 for less-than, equal-to and
  // greater-than respectively.
  // 
  // 以与文件系统相同的方式比较两个字符串。请注意，即使在区分大小写的文件系统上，这些比较也总
  // 是会忽略大小写。如果需要区分大小写的比较，请在此处添加相应的方法。这些方法被写为静态方法，
  // 以便它们也可以用于文件路径的某些部分，例如扩展名。 
  // CompareIgnoreCase() 分别返回 -1,0,1，分别小于，等于和大于。
  static int CompareIgnoreCase(const StringType& string1,
                               const StringType& string2);
  static bool CompareEqualIgnoreCase(const StringType& string1,
                                     const StringType& string2) {
    return CompareIgnoreCase(string1, string2) == 0;
  }
  static bool CompareLessIgnoreCase(const StringType& string1,
                                    const StringType& string2) {
    return CompareIgnoreCase(string1, string2) < 0;
  }

#if defined(OS_MACOSX)
  // Returns the string in the special canonical decomposed form as defined for
  // HFS, which is close to, but not quite, decomposition form D. See
  // http://developer.apple.com/mac/library/technotes/tn/tn1150.html#UnicodeSubtleties
  // for further comments.
  // Returns the epmty string if the conversion failed.
  // 
  // 按照 HFS 定义的特殊规范分解形式返回字符串，该字符串接近但不完全是分解形式 D.
  // 请参见 http://developer.apple.com/mac/library/technotes/tn/tn1150.html#UnicodeSubtleties 
  // 的进一步评论。
  // 
  // 如果转换失败，则返回空字符串。
  static StringType GetHFSDecomposedForm(const FilePath::StringType& string);

  // Special UTF-8 version of FastUnicodeCompare. Cf:
  // http://developer.apple.com/mac/library/technotes/tn/tn1150.html#StringComparisonAlgorithm
  // IMPORTANT: The input strings must be in the special HFS decomposed form!
  // (cf. above GetHFSDecomposedForm method)
  static int HFSFastUnicodeCompare(const StringType& string1,
                                   const StringType& string2);
#endif

#if defined(OS_ANDROID)
  // On android, file selection dialog can return a file with content uri
  // scheme(starting with content://). Content uri needs to be opened with
  // ContentResolver to guarantee that the app has appropriate permissions
  // to access it.
  // Returns true if the path is a content uri, or false otherwise.
  // 
  // 在 android 上，文件选择对话框可以返回带有内容 URI 方案的文件（以 content:// 开头）。
  // 内容 uri 需要使用 ContentResolver 打开以保证应用程序具有访问它的适当权限。如果路径是
  // 内容 uri ，则返回 true ;否则返回 false .
  bool IsContentUri() const;
#endif

 private:
  // Remove trailing separators from this object.  If the path is absolute, it
  // will never be stripped any more than to refer to the absolute root
  // directory, so "////" will become "/", not "".  A leading pair of
  // separators is never stripped, to support alternate roots.  This is used to
  // support UNC paths on Windows.
  // 
  // 从本对象中删除尾分隔符。如果路径是绝对路径，它永远不会被剥离，而是指向绝对根目录，所以 "////" 
  // 将变成 "/" ，而不是 "" 。一对前导分隔符永远不会被剥离，以支持替代根。这用于在 Windows 上支
  // 持 UNC 路径。
  void StripTrailingSeparatorsInternal();

  StringType path_;   // 路径字符串
};

}  // namespace butil

// This is required by googletest to print a readable output on test failures.
// 
// 这是 googletest 在测试失败时打印可读输出所必需的。
BUTIL_EXPORT extern void PrintTo(const butil::FilePath& path, std::ostream* out);

// Macros for string literal initialization of FilePath::CharType[], and for
// using a FilePath::CharType[] in a printf-style format string.
// 
// 用于字符串文字初始化 FilePath::CharType[] 的宏，以及在 printf 样式格式字符串中使用
// FilePath::CharType[] 的宏。
#if defined(OS_POSIX)
#define FILE_PATH_LITERAL(x) x
#define PRFilePath "s"
#define PRFilePathLiteral "%s"
#elif defined(OS_WIN)
#define FILE_PATH_LITERAL(x) L ## x
#define PRFilePath "ls"
#define PRFilePathLiteral L"%ls"
#endif  // OS_WIN

// Provide a hash function so that hash_sets and maps can contain FilePath
// objects.
// 
// 提供一个 std::hash<> 模版特化，以便 hash_sets 和 maps 可以包含 FilePath 对象。
namespace BUTIL_HASH_NAMESPACE {
#if defined(COMPILER_GCC)

template<>
struct hash<butil::FilePath> {
  size_t operator()(const butil::FilePath& f) const {
    return hash<butil::FilePath::StringType>()(f.value());
  }
};

#elif defined(COMPILER_MSVC)

inline size_t hash_value(const butil::FilePath& f) {
  return hash_value(f.value());
}

#endif  // COMPILER

}  // namespace BUTIL_HASH_NAMESPACE

#endif  // BUTIL_FILES_FILE_PATH_H_

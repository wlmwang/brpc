// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adds defines about the platform we're currently building on.
//  Operating System:
//    OS_WIN / OS_MACOSX / OS_LINUX / OS_POSIX (MACOSX or LINUX) /
//    OS_NACL (NACL_SFI or NACL_NONSFI) / OS_NACL_SFI / OS_NACL_NONSFI
//  Compiler:
//    COMPILER_MSVC / COMPILER_GCC
//  Processor:
//    ARCH_CPU_X86 / ARCH_CPU_X86_64 / ARCH_CPU_X86_FAMILY (X86 or X86_64)
//    ARCH_CPU_32_BITS / ARCH_CPU_64_BITS

// 定义构建平台相关宏，包括：
// 操作系统
//    OS_WIN / OS_MACOSX / OS_LINUX / OS_POSIX (MACOSX or LINUX) /
//    OS_NACL (NACL_SFI or NACL_NONSFI) / OS_NACL_SFI / OS_NACL_NONSFI
//    OS_CHROMEOS is set by the build system
// 编译器
//    COMPILER_MSVC / COMPILER_GCC
// 处理器架构
//    ARCH_CPU_X86 / ARCH_CPU_X86_64 / ARCH_CPU_X86_FAMILY (X86 or X86_64)
//    ARCH_CPU_32_BITS / ARCH_CPU_64_BITS

// @tips
// Native Client 是 Google 在浏览器领域推出的一个开源技术，它允许在浏览器内编译
// Web 应用程序，并执行原生的编译好的代码

#ifndef BUTIL_BUILD_CONFIG_H_
#define BUTIL_BUILD_CONFIG_H_

// A set of macros to use for platform detection.
#if defined(__native_client__)
// __native_client__ must be first, so that other OS_ defines are not set.
// 
// __native_client__ 必须在其他 OS_ 宏未定义时检测
#define OS_NACL 1
// OS_NACL comes in two sandboxing technology flavors, SFI or Non-SFI.
// PNaCl toolchain defines __native_client_nonsfi__ macro in Non-SFI build
// mode, while it does not in SFI build mode.
// 
// OS_NACL 的两种沙盒技术分隔： SFI 或 Non-SFI
// 在 Non-SFI 模式中， PNaCl 工具链定义了 __native_client_nonsfi__ 宏
#if defined(__native_client_nonsfi__)
#define OS_NACL_NONSFI
#else
#define OS_NACL_SFI
#endif	// defined(__native_client_nonsfi__)

#elif defined(ANDROID)
#define OS_ANDROID 1

#elif defined(__APPLE__)
// only include TargetConditions after testing ANDROID as some android builds
// on mac don't have this header available and it's not needed unless the target
// is really mac/ios.
// 
// 只有在检测到 __APPLE__ 后才包含 TargetConditionals 。因为某些在 Mac 上构建的
// android 程序没有这个头文件。除非目标是 mac/ios，否则该头也不需要
#include <TargetConditionals.h>
#define OS_MACOSX 1
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define OS_IOS 1
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE

#elif defined(__linux__)
#define OS_LINUX 1
// include a system header to pull in features.h for glibc/uclibc macros.
// 
// 包含 <unistd.h> 系统头以引入 <features.h>， 包含判定使用 glibc/uclibc 库
// 所必要宏。
// 
// @tips
// uclibc 是轻量级 c 库， uclibc 尽可能的兼容 glibc 库。大多数应用程序可以在很小
// 或完全不修改的情况下就可能使用 uclibc 替代 glibc
#include <unistd.h>
#if defined(__GLIBC__) && !defined(__UCLIBC__)
// we really are using glibc, not uClibc pretending to be glibc
// 
// 只使用 GNU 的 glibc 库， 而不是使用 uClibc 伪装成 glibc
#define LIBC_GLIBC 1
#endif	// defined(__GLIBC__) && !defined(__UCLIBC__)

#elif defined(_WIN32)
#define OS_WIN 1
#define TOOLKIT_VIEWS 1

#elif defined(__FreeBSD__)
#define OS_FREEBSD 1

#elif defined(__OpenBSD__)
#define OS_OPENBSD 1

#elif defined(__sun)
#define OS_SOLARIS 1

#elif defined(__QNXNTO__)
#define OS_QNX 1

#else
#error Please add support for your platform in butil/build_config.h
#endif

// 不能同时使用 OpenSSL 和 NSS 证书
#if defined(USE_OPENSSL_CERTS) && defined(USE_NSS_CERTS)
#error Cannot use both OpenSSL and NSS for certificates
#endif

// For access to standard BSD features, use OS_BSD instead of a
// more specific macro.
// 
// 使用 OS_BSD 宏统一指代说明符合标准 BSD 特性平台的不同操作系统
#if defined(OS_FREEBSD) || defined(OS_OPENBSD)
#define OS_BSD 1
#endif

// For access to standard POSIXish features, use OS_POSIX instead of a
// more specific macro.
// 
// 使用 OS_POSIX 宏统一指代说明符合标准 POSIXish 特性平台的不同操作系统
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_FREEBSD) ||     \
    defined(OS_OPENBSD) || defined(OS_SOLARIS) || defined(OS_ANDROID) ||  \
    defined(OS_NACL) || defined(OS_QNX)
#define OS_POSIX 1
#endif

// Use tcmalloc
// 
// 使用 tcmalloc 库
#if (defined(OS_WIN) || defined(OS_LINUX) || defined(OS_ANDROID)) && \
    !defined(NO_TCMALLOC)
#define USE_TCMALLOC 1
#endif

// Compiler detection.
// 
// 检测定义 Compiler 宏
// 
// @tips
// _MSC_VER 宏为 Microsoft 的 C 编译器的版本
// MSVC++ 11.0 _MSC_VER = 1700 (Visual Studio 2011) 
// MSVC++ 10.0 _MSC_VER = 1600 (Visual Studio 2010) 
// MSVC++ 9.0  _MSC_VER = 1500 (Visual Studio 2008) 
// ...
#if defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in butil/build_config.h
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
//   
// 检测定义 Processor architecture 宏。gcc 可查考 "echo | gcc -E -dM -" 输出
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARMEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__aarch64__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARM64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__pnacl__)
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__MIPSEL__)
#if defined(__LP64__)
#define ARCH_CPU_MIPS64_FAMILY 1
#define ARCH_CPU_MIPS64EL 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPSEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#endif
#else
#error Please add support for your architecture in butil/build_config.h
#endif

// Type detection for wchar_t.
// 
// wchar_t 类型检测。
// 
// @tips
// win 系统只处理基本 unicode(UTF-16/UCS-2) 字符集
#if defined(OS_WIN)
#define WCHAR_T_IS_UTF16
// 默认 POSIX 平台的 gcc 编译器 wchar_t 都是 32 位的
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && \
    defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define WCHAR_T_IS_UTF32

#elif defined(OS_POSIX) && defined(COMPILER_GCC) && \
    defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
// On Posix, we'll detect short wchar_t, but projects aren't guaranteed to
// compile in this mode (in particular, Chrome doesn't). This is intended for
// other projects using base who manage their own dependencies and make sure
// short wchar works for them.
// 
// POSIX 上，可能会检测到 short wchar_t ，但项目不能保证可以编译（尤其是 Chrome 不可以）
// 使用的人来保证他们的项目适用于 short wchar
#define WCHAR_T_IS_UTF16
#else
#error Please add support for your compiler in butil/build_config.h
#endif

// ANDROID 平台 std::string 的迭代器与 const char* 特殊关系
#if defined(OS_ANDROID)
// The compiler thinks std::string::const_iterator and "const char*" are
// equivalent types.
// 
// 编译器认为 std::string::const_iterator 和 "const char*" 是等效类型
#define STD_STRING_ITERATOR_IS_CHAR_POINTER
// The compiler thinks butil::string16::const_iterator and "char16*" are
// equivalent types.
// 
// 编译器认为 std::string16::const_iterator 和 "char16*" 是等效类型
#define BUTIL_STRING16_ITERATOR_IS_CHAR16_POINTER
#endif

// C++11 检测
#if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L
#define BUTIL_CXX11_ENABLED 1
#endif

// 低版本编译器简单定义 nullptr 宏
#if !defined(BUTIL_CXX11_ENABLED)
#define nullptr NULL
#endif

#define HAVE_DLADDR

#endif  // BUTIL_BUILD_CONFIG_H_

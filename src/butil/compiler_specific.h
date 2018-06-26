// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 定义编译器相关预编译指令宏（如，修改编译器的警告消息的行为、是否开启编译优化、内联
// 函数声明设置、内存对齐设置、类继承设置等等）

#ifndef BUTIL_COMPILER_SPECIFIC_H_
#define BUTIL_COMPILER_SPECIFIC_H_

#include "butil/build_config.h"

#if defined(COMPILER_MSVC)

// Macros for suppressing and disabling warnings on MSVC.
// 
// 抑制和禁用 MSVC 上的警告的宏
//
// Warning numbers are enumerated at:
// http://msdn.microsoft.com/en-us/library/8x5x43k7(VS.80).aspx
//
// The warning pragma:
// http://msdn.microsoft.com/en-us/library/2c8f766e(VS.80).aspx
//
// Using __pragma instead of #pragma inside macros:
// http://msdn.microsoft.com/en-us/library/d9x1s805.aspx


// @tips
// #pragma warning 可有选择性的修改编译器的警告消息的行为。只对当前文件有效（对
// 于 .h ，对包含它的 cpp 也是有效的），而不是对整个工程的所有文件有效，当该文件
// 编译结束，设置也就失去作用。语法：
// #pragma warning(warning-specifier:warning-number-list [;warning-specifier:warning-number-list...])
// #pragma warning(push [,n])
// #pragma warning(pop)
// 
// 主要用到的警告表示，有如下几个:
// once:	只显示一次(警告/错误等)消息
// default:	重置编译器的警告行为到默认状态
// disable:	禁止指定的警告信息
// error:	将指定的警告信息作为错误报告
// 1,2,3,4:	四个警告级别
// 
// e.g.
// #pragma warning(disable:4507 34; once:4385; error:164)
// 等价于：  
// #pragma warning(disable:4507 34)	// 不显示 4507 和 34 号警告信息
// #pragma warning(once:4385)		// 4385 号警告信息仅报告一次
// #pragma warning(error:164)		// 把 164 号警告信息作为一个错误
// 
// 
// #pragma warning(push [,n])	// 这里 n 代表一个警告等级(1---4)
// #pragma warning(pop)  
// 
// #pragma warning(push)	保存所有警告信息的现有的警告状态
// #pragma warning(push, n)	保存所有警告信息的现有的警告状态，并且把全局警告等级设定为 n
// #pragma warning(pop)		从栈中弹出最后一个警告信息，在push和pop之间所作的任何报警相关
// 设置都将失效
// 
// e.g.
// #pragma warning(push)  
// #pragma warning(disable: 4705)  
// #pragma warning(disable: 4706)  
// #pragma warning(disable: 4707)  
// #pragma warning(pop)
// 在这段代码的最后，重新保存所有的警告信息(包括 4705,4706 和 4707 失效)
// 
// 
// 1. 在使用标准 C++ 进行编程的时候经常会得到很多的警告信息,而这些警告信息都是不必要
// 的提示,所以我们可以使用 #pragma warning(disable:4786) 来禁止该类型的警告
// 
// 2. 在 vc 中使用 ADO 的时候也会得到不必要的警告信息,这个时候我们可以通过
// #pragma warning(disable:4146) 来消除该类型的警告信息


// MSVC_SUPPRESS_WARNING disables warning |n| for the remainder of the line and
// for the next line of the source file.
#define MSVC_SUPPRESS_WARNING(n) __pragma(warning(suppress:n))

// MSVC_PUSH_DISABLE_WARNING pushes |n| onto a stack of warnings to be disabled.
// The warning remains disabled until popped by MSVC_POP_WARNING.
#define MSVC_PUSH_DISABLE_WARNING(n) __pragma(warning(push)) \
                                     __pragma(warning(disable:n))

// MSVC_PUSH_WARNING_LEVEL pushes |n| as the global warning level.  The level
// remains in effect until popped by MSVC_POP_WARNING().  Use 0 to disable all
// warnings.
#define MSVC_PUSH_WARNING_LEVEL(n) __pragma(warning(push, n))

// Pop effects of innermost MSVC_PUSH_* macro.
#define MSVC_POP_WARNING() __pragma(warning(pop))

// @tips
// #pragma optimize("[optimization-list]",{on| off}), 仅有 VC++ 专业版和企业版支持.
// 指定在函数层次执行的优化。optimize 编译选项必须在函数外出现，并且在该编译指示出现以后的
// 第一个函数定义开始起作用。on 和 off 参数打开或关闭在 optimization-list 指定的选项。
// 
// optimization-list 和在 /O 编译程序选项中使用的是相同的字母。
// 用空字符串（""）的 optimize 编译指示是一种特别形式。它要么关闭所有的优化选项，要么恢复
// 它们到原始（或默认）的设定。

#define MSVC_DISABLE_OPTIMIZE() __pragma(optimize("", off))
#define MSVC_ENABLE_OPTIMIZE() __pragma(optimize("", on))

// Allows exporting a class that inherits from a non-exported base class.
// This uses suppress instead of push/pop because the delimiter after the
// declaration (either "," or "{") has to be placed before the pop macro.
// 
// 允许导出从非导出(non-exported)基类继承的类。这使用 suppress 而不是 push/pop，因为声
// 明后的分隔符（"," 或者 "{" ）必须放在 pop 宏之前
//
// Example usage:
// class EXPORT_API Foo : NON_EXPORTED_BASE(public Bar) {
//
// MSVC Compiler warning C4275:
// non dll-interface class 'Bar' used as base for dll-interface class 'Foo'.
// Note that this is intended to be used only when no access to the base class'
// static data is done through derived classes or inline methods. For more info,
// see http://msdn.microsoft.com/en-us/library/3tdb471s(VS.80).aspx
#define NON_EXPORTED_BASE(code) MSVC_SUPPRESS_WARNING(4275) \
                                code

#else  // Not MSVC

#define MSVC_SUPPRESS_WARNING(n)
#define MSVC_PUSH_DISABLE_WARNING(n)
#define MSVC_PUSH_WARNING_LEVEL(n)
#define MSVC_POP_WARNING()
#define MSVC_DISABLE_OPTIMIZE()
#define MSVC_ENABLE_OPTIMIZE()
#define NON_EXPORTED_BASE(code) code

#endif  // COMPILER_MSVC


// The C++ standard requires that static const members have an out-of-class
// definition (in a single compilation unit), but MSVC chokes on this (when
// language extensions, which are required, are enabled). (You're only likely to
// notice the need for a definition if you take the address of the member or,
// more commonly, pass it to a function that takes it as a reference argument --
// probably an STL function.) This macro makes MSVC do the right thing. See
// http://msdn.microsoft.com/en-us/library/34h23df8(v=vs.100).aspx for more
// information. Use like:
//
// @todo
// C++ 标准要求类 static const 成员变量类内初始化后，在类外也需要声明（比如 .h/.cc 文件）。
// selectany 也可以让我们在 .h 文件中初始化一个全局变量而不是只能放在 .cpp 中。比如有一个
// 类，其中有一个静态变量，那么我们可以在 .h 中通过 __declspec(selectany) type class::variable = value;
// 这样的代码来初始化这个全局变量。即时该 .h 被多次 include ，链接器也会为我们剔除多重定
// 义的错误。对于 template 的编程会有很多便利。
// 格式： __declspec(selectany) declarator
// 
//
// In .h file:
//   struct Foo {
//     static const int kBar = 5;
//   };
//
// In .cc file:
//   STATIC_CONST_MEMBER_DEFINITION const int Foo::kBar;
#if defined(COMPILER_MSVC)
#define STATIC_CONST_MEMBER_DEFINITION __declspec(selectany)
#else
#define STATIC_CONST_MEMBER_DEFINITION
#endif

// Annotate a variable indicating it's ok if the variable is not used.
// (Typically used to silence a compiler warning when the assignment
// is important for some other reason.)
// Use like:
//   int x ALLOW_UNUSED = ...;
//   
// 注释说明一个未使用变量是 ok 的，用于抑制警告。
#if defined(COMPILER_GCC)
#define ALLOW_UNUSED __attribute__((unused))
#else
#define ALLOW_UNUSED
#endif

// Annotate a function indicating it should not be inlined.
// Use like:
//   NOINLINE void DoStuff() { ... }
//   
// 注释说明一个函数不应该被 inline 。（方便调试）
#if defined(COMPILER_GCC)
#define NOINLINE __attribute__((noinline))
#elif defined(COMPILER_MSVC)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif

// 强制 inline
#ifndef BUTIL_FORCE_INLINE
#if defined(COMPILER_MSVC)
#define BUTIL_FORCE_INLINE    __forceinline
#else
#define BUTIL_FORCE_INLINE inline __attribute__((always_inline))
#endif
#endif  // BUTIL_FORCE_INLINE

// Specify memory alignment for structs, classes, etc.
// Use like:
//   class ALIGNAS(16) MyClass { ... }
//   ALIGNAS(16) int array[4];
//   
// 指定 structs, classes, etc. 内存对齐字节长度。
// C++11 起，可使用 alignas 运算符设定内存对齐长度。
#if defined(COMPILER_MSVC)
#define ALIGNAS(byte_alignment) __declspec(align(byte_alignment))
#elif defined(COMPILER_GCC)
#define ALIGNAS(byte_alignment) __attribute__((aligned(byte_alignment)))
#endif

// Return the byte alignment of the given type (available at compile time).  Use
// sizeof(type) prior to checking __alignof to workaround Visual C++ bug:
// http://goo.gl/isH0C
// Use like:
//   ALIGNOF(int32_t)  // this would be 4
// 
// 返回给定类型内存对齐长度。
// C++11 起，可使用 alignof 运算符返回内存对齐长度。
#if defined(COMPILER_MSVC)
#define ALIGNOF(type) (sizeof(type) - sizeof(type) + __alignof(type))
#elif defined(COMPILER_GCC)
#define ALIGNOF(type) __alignof__(type)
#endif

// Annotate a virtual method indicating it must be overriding a virtual
// method in the parent class.
// Use like:
//   virtual void foo() OVERRIDE;
// 
// 注释说明一个虚函数，子类中必须重写其基类。
// C++11 起，可使用 override 关键字设定。
#if defined(__clang__) || defined(COMPILER_MSVC)
#define OVERRIDE override
#elif defined(COMPILER_GCC) && __cplusplus >= 201103 && \
      (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40700
// GCC 4.7 supports explicit virtual overrides when C++11 support is enabled.
#define OVERRIDE override
#else
#define OVERRIDE
#endif

// Annotate a virtual method indicating that subclasses must not override it,
// or annotate a class to indicate that it cannot be subclassed.
// Use like:
//   virtual void foo() FINAL;
//   class B FINAL : public A {};
// 
// 注释说明一个虚方法，子类不能重写。或者注释说明一个类，不能被继承。
// C++11 起，可使用 final 关键字设定。
#if defined(__clang__) || defined(COMPILER_MSVC)
#define FINAL final
#elif defined(COMPILER_GCC) && __cplusplus >= 201103 && \
      (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40700
// GCC 4.7 supports explicit virtual overrides when C++11 support is enabled.
#define FINAL final
#else
#define FINAL
#endif

// Annotate a function indicating the caller must examine the return value.
// Use like:
//   int foo() WARN_UNUSED_RESULT;
// To explicitly ignore a result, see |ignore_result()| in "butil/basictypes.h".
// FIXME(gejun): GCC 3.4 report "unused" variable incorrectly (actually used).
// 
// 注释说明一个函数调用，调用者必须检查其返回值。
#if defined(COMPILER_GCC) && __cplusplus >= 201103 && \
      (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40700
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define WARN_UNUSED_RESULT
#endif

// Tell the compiler a function is using a printf-style format string.
// |format_param| is the one-based index of the format string parameter;
// |dots_param| is the one-based index of the "..." parameter.
// For v*printf functions (which take a va_list), pass 0 for dots_param.
// (This is undocumented but matches what the system C headers do.)
// 
// 注释说明某函数正使用 printf-style 格式的字符串参数，以便编译器进行检查。
#if defined(COMPILER_GCC)
#define PRINTF_FORMAT(format_param, dots_param) \
    __attribute__((format(printf, format_param, dots_param)))
#else
#define PRINTF_FORMAT(format_param, dots_param)
#endif

// WPRINTF_FORMAT is the same, but for wide format strings.
// This doesn't appear to yet be implemented in any compiler.
// See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=38308 .
// 
// PRINTF_FORMAT 的宽字符版本定义为空。
#define WPRINTF_FORMAT(format_param, dots_param)
// If available, it would look like:
//   __attribute__((format(wprintf, format_param, dots_param)))

// MemorySanitizer annotations.
// 
// @todo
#if defined(MEMORY_SANITIZER) && !defined(OS_NACL)
#include <sanitizer/msan_interface.h>

// Mark a memory region fully initialized.
// Use this to annotate code that deliberately reads uninitialized data, for
// example a GC scavenging root set pointers from the stack.
// 
// @todo
#define MSAN_UNPOISON(p, s)  __msan_unpoison(p, s)
#else  // MEMORY_SANITIZER
#define MSAN_UNPOISON(p, s)
#endif  // MEMORY_SANITIZER

// Macro useful for writing cross-platform function pointers.
// 
// 交叉平台编译二进制目标，声明函数调用规范。
#if !defined(CDECL)
#if defined(OS_WIN)
#define CDECL __cdecl
#else  // defined(OS_WIN)
#define CDECL
#endif  // defined(OS_WIN)
#endif  // !defined(CDECL)

// Mark a branch likely or unlikely to be true.
// We can't remove the BAIDU_ prefix because the name is likely to conflict,
// namely kylin already has the macro.
// 
// @tips
// __builtin_expect 是 GCC(version >= 2.96）提供给程序员使用的。目的是将 "分支转移" 的
// 信息提供给编译器，这样编译器可以对代码进行优化，以减少指令跳转带来的性能下降。即可以让 CPU
// 提前装载 likely 分支指令，以优化分支代码效率。
// unlikely 表示你可以确认该条件是极少发生的，相反 likely 表示该条件多数情况下会发生。
// 
// __builtin_expect() 中，expr 为一个整型表达式（也是函数返回值）， 第二个参数为一个编译期常量。
// 语义：期望 exp 表达式的值等于常量 c ，从而 GCC 为你优化程序，将符合这个条件的分支放在合适的地方，
// 因为这个程序只提供了整型表达式，所以如果你要优化其他类型的表达式，可以采用指针的形式
// 
// Use like:
// int x, y; 
// if (BAIDU_UNLIKELY(x > 0))
//    y = 1;
// else 
//    y = -1;
#if defined(COMPILER_GCC)
#  if defined(__cplusplus)
#    define BAIDU_LIKELY(expr) (__builtin_expect((bool)(expr), true))
#    define BAIDU_UNLIKELY(expr) (__builtin_expect((bool)(expr), false))
#  else
#    define BAIDU_LIKELY(expr) (__builtin_expect(!!(expr), 1))
#    define BAIDU_UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#  endif
#else
#  define BAIDU_LIKELY(expr) (expr)
#  define BAIDU_UNLIKELY(expr) (expr)
#endif

// BAIDU_DEPRECATED void dont_call_me_anymore(int arg);
// ...
// warning: 'void dont_call_me_anymore(int)' is deprecated
// 
// 可以使用 deprecated 变量属性声明不建议使用的变量/函数，而不会导致编译器发出任何警告或错误。
// 但是，对 deprecated 变量/函数的任何访问都会生成警告，但仍会进行编译。警告指出了使用和定义
// 变量/函数的位置。
#if defined(COMPILER_GCC)
# define BAIDU_DEPRECATED __attribute__((deprecated))
#elif defined(COMPILER_MSVC)
# define BAIDU_DEPRECATED __declspec(deprecated)
#else
# define BAIDU_DEPRECATED
#endif

// Mark function as weak. This is GCC only feature.
// 
// 标记一个函数为弱符号（仅 GCC 特性）。当有两个函数同名时，则使用强符号（也叫全局符号）来代
// 替弱符号
// 
// @tips
// 若两个或两个以上全局符号（函数或变量名）名字一样，而其中之一声明为 weak （弱符号），则这些
// 全局符号不会引发重定义错误，链接器会忽略弱符号，去使用普通的全局符号来解析所有对这些符号的
// 引用，但当普通的全局符号不可用时，链接器会使用弱符号。
// 当有函数或变量名可能被用户覆盖时，该函数或变量名可以声明为一个弱符号。当 weak 和 alias 属
// 性连用时，还可以声明弱别名。
// 
// Use like:
// 
// \file weak.c
// #include <stdio.h>
// extern void symbol1() __attribute__((weak));
// void symbol1()
// {
//     printf("%s.%s\n", __FILE__, __FUNCTION__);
// }
// 
// int main()
// {
//     symbol1();
//     return 0;
// }
// 
// \file strong.c
// #include <stdio.h>
// void symbol1()
// {
//    printf("%s.%s\n", __FILE__, __FUNCTION__);
// }
// 
// 
// $ gcc -o weakstrong weak.c
// $ ./weakstrong
// 输出 weak.c symbol1
// 
// $ gcc -o weakstrong weak.c strong.c
// $ ./weakstrong
// 输出：strong.c symbol1
#if defined(COMPILER_GCC)
# define BAIDU_WEAK __attribute__((weak))
#else
# define BAIDU_WEAK
#endif

// Cacheline related --------------------------------------
// 
// Cacheline 相关配置
#define BAIDU_CACHELINE_SIZE 64

// BAIDU_CACHELINE_ALIGNMENT 声明类时，指定 BAIDU_CACHELINE_SIZE 内存对齐长度。
// C++11 起，可使用 alignas 运算符设定内存对齐长度。
#ifdef _MSC_VER
# define BAIDU_CACHELINE_ALIGNMENT __declspec(align(BAIDU_CACHELINE_SIZE))
#endif /* _MSC_VER */

#ifdef __GNUC__
# define BAIDU_CACHELINE_ALIGNMENT __attribute__((aligned(BAIDU_CACHELINE_SIZE)))
#endif /* __GNUC__ */

#ifndef BAIDU_CACHELINE_ALIGNMENT
# define BAIDU_CACHELINE_ALIGNMENT /*BAIDU_CACHELINE_ALIGNMENT*/
#endif

// 声明指定函数不抛出异常。C++11 起，可使用 noexcept 关键字设定。
// 
// @tips
// extern const nothrow_t nothrow;	// nothrow为std内预定义的对象。
// 
// new(std::nothrow) T; 无异常 new(placement new) 操作。
#ifndef BAIDU_NOEXCEPT
# if defined(BUTIL_CXX11_ENABLED)
#  define BAIDU_NOEXCEPT noexcept
# else
#  define BAIDU_NOEXCEPT
# endif
#endif

#endif  // BUTIL_COMPILER_SPECIFIC_H_

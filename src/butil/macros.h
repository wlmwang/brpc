// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains macros and macro-like constructs (e.g., templates) that
// are commonly used throughout Chromium source. (It may also contain things
// that are closely related to things that are commonly used that belong in this
// file.)
// 
// 该文件包含通常在整个 Chromium 源代码中使用的宏和类宏结构（例如，模板）（它也可能包含与
// 这个文件中常用东西密切相关的东西）

#ifndef BUTIL_MACROS_H_
#define BUTIL_MACROS_H_

#include <stddef.h>  // For size_t.
#include <string.h>  // For memcpy.

#include "butil/compiler_specific.h"  // For ALLOW_UNUSED.

// There must be many copy-paste versions of these macros which are same
// things, undefine them to avoid conflict.
#undef DISALLOW_COPY
#undef DISALLOW_ASSIGN
#undef DISALLOW_COPY_AND_ASSIGN
#undef DISALLOW_EVIL_CONSTRUCTORS
#undef DISALLOW_IMPLICIT_CONSTRUCTORS

// C++11 中支持 delete 关键字显式的禁用某个函数
#if !defined(BUTIL_CXX11_ENABLED)
#define BUTIL_DELETE_FUNCTION(decl) decl
#else
#define BUTIL_DELETE_FUNCTION(decl) decl = delete
#endif

// Put this in the private: declarations for a class to be uncopyable.
// 
// 放入 private ，声明禁用复制构造
#define DISALLOW_COPY(TypeName)                         \
    BUTIL_DELETE_FUNCTION(TypeName(const TypeName&))

// Put this in the private: declarations for a class to be unassignable.
// 
// 放入 private ，声明禁用赋值运算
#define DISALLOW_ASSIGN(TypeName)                               \
    BUTIL_DELETE_FUNCTION(void operator=(const TypeName&))

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
// 
// 放入 private ，声明禁用复制构造、赋值运算
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                      \
    BUTIL_DELETE_FUNCTION(TypeName(const TypeName&));            \
    BUTIL_DELETE_FUNCTION(void operator=(const TypeName&))

// An older, deprecated, politically incorrect name for the above.
// NOTE: The usage of this macro was banned from our code base, but some
// third_party libraries are yet using it.
// TODO(tfarina): Figure out how to fix the usage of this macro in the
// third_party libraries and get rid of it.
#define DISALLOW_EVIL_CONSTRUCTORS(TypeName) DISALLOW_COPY_AND_ASSIGN(TypeName)

// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
// 
// 放入 private ，声明禁用默认构造、复制构造、赋值运算。常用用在仅包含静态方法的类。
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
    BUTIL_DELETE_FUNCTION(TypeName());            \
    DISALLOW_COPY_AND_ASSIGN(TypeName)

// Concatenate numbers in c/c++ macros.
// 
// 把两个宏形参 token，串联在一起形成一个新的 token
#ifndef BAIDU_CONCAT
# define BAIDU_CONCAT(a, b) BAIDU_CONCAT_HELPER(a, b)
# define BAIDU_CONCAT_HELPER(a, b) a##b
#endif

#undef arraysize
// The arraysize(arr) macro returns the # of elements in an array arr.
// The expression is a compile-time constant, and therefore can be
// used in defining new arrays, for example.  If you use arraysize on
// a pointer by mistake, you will get a compile-time error.
//
// One caveat is that arraysize() doesn't accept any array of an
// anonymous type or a type defined inside a function.  In these rare
// cases, you have to use the unsafe ARRAYSIZE_UNSAFE() macro below.  This is
// due to a limitation in C++'s template system.  The limitation might
// eventually be removed, but it hasn't happened yet.
// 
// arraysize(arr) 宏返回 array 数组元素的个数，是一个编译器常量，因此可用于定义
// 新数组，如果错误地在指针上使用，将会收到编译时错误。
// 
// 值得注意的是， arraysize() 不接受任何匿名类型的数组或函数内定义的类型。在这些罕见
// 的情况下，必须使用不安全的 ARRAYSIZE_UNSAFE() 宏。这是由于 C++ 模板系统的限制，
// 限制可能最终被删除，但还还不是现在。

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.    
//  
// ArraySizeHelper 是一个函数，该函数具有一个类型为引用了类型为 T、长度为 N 的数组的
// 参数 array ， 返回一个类型为 char 长度为 N 的数组
namespace butil {
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
}

// That gcc wants both of these prototypes seems mysterious. VC, for
// its part, can't decide which to use (another mystery). Matching of
// template overloads: the final frontier.
// 
// @todo
// gcc 还需要以下模板函数的原型(谜?)
#ifndef _MSC_VER
namespace butil {
template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
}
#endif

// 计算数组个数
#define arraysize(array) (sizeof(::butil::ArraySizeHelper(array)))

// gejun: Following macro was used in other modules.
#undef ARRAY_SIZE
#define ARRAY_SIZE(array) arraysize(array)

// ARRAYSIZE_UNSAFE performs essentially the same calculation as arraysize,
// but can be used on anonymous types or types defined inside
// functions.  It's less safe than arraysize as it accepts some
// (although not all) pointers.  Therefore, you should use arraysize
// whenever possible.
//
// The expression ARRAYSIZE_UNSAFE(a) is a compile-time constant of type
// size_t.
//
// ARRAYSIZE_UNSAFE catches a few type errors.  If you see a compiler error
//
//   "warning: division by zero in ..."
//
// when using ARRAYSIZE_UNSAFE, you are (wrongfully) giving it a pointer.
// You should only use ARRAYSIZE_UNSAFE on statically allocated arrays.
//
// The following comments are on the implementation details, and can
// be ignored by the users.
//
// ARRAYSIZE_UNSAFE(arr) works by inspecting sizeof(arr) (the # of bytes in
// the array) and sizeof(*(arr)) (the # of bytes in one array
// element).  If the former is divisible by the latter, perhaps arr is
// indeed an array, in which case the division result is the # of
// elements in the array.  Otherwise, arr cannot possibly be an array,
// and we generate a compiler error to prevent the code from
// compiling.
//
// Since the size of bool is implementation-defined, we need to cast
// !(sizeof(a) & sizeof(*(a))) to size_t in order to ensure the final
// result has type size_t.
//
// This macro is not perfect as it wrongfully accepts certain
// pointers, namely where the pointer size is divisible by the pointee
// size.  Since all our code has to go through a 32-bit compiler,
// where a pointer is 4 bytes, this means all pointers to a type whose
// size is 3 or greater than 4 will be (righteously) rejected.
// 
// ARRAYSIZE_UNSAFE 执行与 arraysize 基本相同的计算，但可用于匿名类型或函数内定义的
// 类型。它在接受一些（虽然不是全部）指针参数时，没有 arraysize 安全（计算不正确），因
// 为它错误地接受某些指针，并当指针大小可以被指针指向的数据整除时，结算结果就是错误的。
// 而且编译器不会编译错误。（注意，因为我们所有的代码必须经过一个 32位编译器，其中一个指
// 针是 4 个字节，这意味着所有指向大小为 3 或大于 4 的类型的指针将被正确拒绝）
// 
// ARRAYSIZE_UNSAFE(arr) 通过检查 sizeof(arr) 和 sizeof(*(arr)) 进行工作。如果前
// 者可以被后者整除， arr 可能是一个数组，在这种情况下，除法结果是数组中的元素个数。否则，
// arr 不可能是一个数组，并且我们生成一个编译器错误以防止编译代码。由于 bool 的大小由实
// 现定义的，所以我们需要将 !(sizeof(a) & sizeof(*(a))) 转换为 size_t 以确保最终结
// 果的类型为 size_t
#undef ARRAYSIZE_UNSAFE
#define ARRAYSIZE_UNSAFE(a) \
    ((sizeof(a) / sizeof(*(a))) / \
     static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

// Use implicit_cast as a safe version of static_cast or const_cast
// for upcasting in the type hierarchy (i.e. casting a pointer to Foo
// to a pointer to SuperclassOfFoo or casting a pointer to Foo to
// a const pointer to Foo).
// When you use implicit_cast, the compiler checks that the cast is safe.
// Such explicit implicit_casts are necessary in surprisingly many
// situations where C++ demands an exact type match instead of an
// argument type convertible to a target type.
// 
//
// implicit_cast 可作为 static_cast 或者 const_cast 在类型层次结构中进行向上转
// 换（子类向基类）的安全版本。比如，将指向 Foo 的指针转换为指向 SuperclassOfFoo 
// 的指针，或将指向 Foo 的指针转换为指向 Foo 的 const 指针
// 
// 当你使用 implicit_cast 时，编译器会检查该转换是否安全。
// 令人惊讶的是，在 C++ 需要一个精确的类型匹配，而不是可转换为目标类型的参数类型时，
// 这种显式的 implicit_cast 是必需的
// i.e.
// class Top{};
// class MiddleA :public Top{};
// class MiddleB :public Top{};
// class Bottom :public MiddleA, public MiddleB {};
//
// void Function(MiddleA& A)
// {
//     std::cout<<"MiddleA Function"<<std::endl;
// }
// void Function(MiddleB& B)
// {
//     std::cout<<"MiddleB Function"<<std::endl;
// }
// 
// int main()
// {
//     Bottom bottom;
//     Function(bottom);
//     return 0;
// }
// 
// 此时编译就会出错，因为 Function(bottom) 调用出现了歧义。
// 1. 若代码修改成以下两种之一即可编译：
// Function(static_cast<MiddleA &>(bottom));
// Function(static_cast<MiddleB &>(bottom));
// 
// 2. 若代码修改成：
// Top top;
// Function(static_cast<MiddleB &>top);
// 就会出现编译可以通过，但是在运行时却崩溃了！
// 
// 3. 若代码中的转换使用 implicit_cast ，则问题在编译期即可发现问题
// Top top;
// Function(implicit_cast<MiddleB &>top);  // compile failed
//
//
// The From type can be inferred, so the preferred syntax for using
// implicit_cast is the same as for static_cast etc.:
//
//   implicit_cast<ToType>(expr)
//
// implicit_cast would have been part of the C++ standard library,
// but the proposal was submitted too late.  It will probably make
// its way into the language in the future.
// 
// 
// @tips
// 一. static_cast      强制隐型转换，最接近于 C-style 的转换
// 是最常用的类型转换，允许执行任意的隐式转换和相反转换动作（即使它是不允许隐式的）
// 应用到类的指针上，意思是说它允许子类类型的指针转换为父类类型的指针（这是一个有效
// 的隐式转换），同时，也能够执行相反动作：转换父类为它的子类。
// static_cast 可以将 non-const 对象转型为 const 对象，但是它不能将一个 const 
// 对象转型为 non-const 对象
// i.e.
// class Base {};
// class Derived : public Base {};
// Base *a = new Base;
// Derived *b = static_cast<Derived *>(a);
// 
// double d = 3.14159265;
// int    i = static_cast<int>(d);
// 
// 二. const_cast 安全移除 cv 约束
// 从名字可以看出和 const 有关，这个转换的作用是去除或添加 const 特性，它可以将一
// 个 const 变量转换为非 const 变量，或将一个非 const 变量转换为 const 变量
// const_cast 也能改变一个类型的 volatile qualifier
// i.e.
// class C {};
// const C *a = new C;
// C *b = const_cast<C*>(a);
// 
// 三. dynamic_cast     安全的向下转型
// 只用于对象的指针和引用，被转换的类型必须是多态（即有虚函数）的。当用于多态类型时，
// 它允许任意的隐式类型转换以及相反过程，与 static_cast 不同，在后一种情况里（隐式
// 转换的相反过程）， dynamic_cast 会检查操作是否有效，也就是说，它会检查转换是否
// 会返回一个被请求的有效的完整对象。如果被转换的指针不是一个被请求的有效完整的对象
// 指针，返回值为 NULL
// 如果一个引用类型执行了类型转换并且这个转换是不可能的，一个 bad_cast 的异常类型
// 被抛出
// i.e.
// class Base { virtual dummy() {} };
// class Derived : public Base {};
// Base* b1 = new Derived;
// Base* b2 = new Base;
// Derived* d1 = dynamic_cast<Derived*>(b1);    // succeeds
// Derived* d2 = dynamic_cast<Derived*>(b2);    // failed: returns 'NULL'
// Derived d3 = dynamic_cast<Derived &*>(b1);   // succeeds
// Derived d4 = dynamic_cast<Derived &*>(b2);   // failed: exception thrown
// 
// 四. interpret_cast 低级的类型转换
// interpret 意思为翻译、解释，它可以将数据的二进制格式重新解释，结果依赖机器。
// interpret_cast 操作符能够在非相关的类型之间转换，操作结果只是简单的从一个指针到别
// 的指针的值的二进制拷贝
// interpret_cast 可以将一个指针转换为其它类型的指针，它也允许从一个指针转换为整数类
// 型，反之亦然
// i.e.
// class A {};
// class B {};
// A * a = new A;
// B * b = reinterpret_cast<B*>(a);
// 
// implicit_cast
// 表示隐式类型转换，只有可以隐式转换的地方才可以用它，即 implicit_cast 只能执行 upcasting
// implicit_cast 会是标准的一部分 boost::implicit_cast
// Use like:
// auto x = implicit_cast<ToType>(expr);
namespace butil {
template<typename To, typename From>
inline To implicit_cast(From const &f) {
  return f;
}
}

#if defined(BUTIL_CXX11_ENABLED)

// C++11 supports compile-time assertion directly
// 
// 编译期静态断言
// 若 expr 返回 true ，则此声明没有效果。否则发布一个编译时错误，而且若存在 msg， 则其文
// 本被包含于诊断消息中。msg 会被自动转换为字符串
// 
// Use like:
// static_assert(false, "file not exists");
// 可能的输出：
// error: static assertion failed: file not exists
#define BAIDU_CASSERT(expr, msg) static_assert(expr, #msg)

#else

// Assert constant boolean expressions at compile-time
// Params:
//   expr     the constant expression to be checked
//   msg      an error infomation conforming name conventions of C/C++
//            variables(alphabets/numbers/underscores, no blanks). For
//            example "cannot_accept_a_number_bigger_than_128" is valid
//            while "this number is out-of-range" is illegal.
//
// when an asssertion like "BAIDU_CASSERT(false, you_should_not_be_here)"
// breaks, a compilation error is printed:
//   
//   foo.cpp:401: error: enumerator value for `you_should_not_be_here___19' not
//   integer constant
//
// You can call BAIDU_CASSERT at global scope, inside a class or a function
// 
//   BAIDU_CASSERT(false, you_should_not_be_here);
//   int main () { ... }
//
//   struct Foo {
//       BAIDU_CASSERT(1 == 0, Never_equals);
//   };
//
//   int bar(...)
//   {
//       BAIDU_CASSERT (value < 10, invalid_value);
//   }
//
// expr 为编译器即可确定的静态标量类型表达式.
// msg 必须为符合 C/C++ 变量名称约定的错误信息（字母/数字/下划线，无空格）.
// 
// Use like:
// BAIDU_CASSERT(false, you_should_not_be_here)
// 可能输出：
// foo.cpp:401: error: enumerator value for `LINE_19__you_should_not_be_here' not 
// integer constant
namespace butil {
template <bool> struct CAssert { static const int x = 1; };
template <> struct CAssert<false> { static const char * x; };
}

#define BAIDU_CASSERT(expr, msg)                                \
    enum { BAIDU_CONCAT(BAIDU_CONCAT(LINE_, __LINE__), __##msg) \
           = ::butil::CAssert<!!(expr)>::x };

#endif  // BUTIL_CXX11_ENABLED

// The impl. of chrome does not work for offsetof(Object, private_filed)
#undef COMPILE_ASSERT
#define COMPILE_ASSERT(expr, msg)  BAIDU_CASSERT(expr, msg)

// bit_cast<Dest,Source> is a template function that implements the
// equivalent of "*reinterpret_cast<Dest*>(&source)".  We need this in
// very low-level functions like the protobuf library and fast math
// support.
// 
//   float f = 3.14159265358979;
//   int i = bit_cast<int32_t>(f);
//   // i = 0x40490fdb
//
// The classical address-casting method is:
//
//   // 未定义的行为
//   // WRONG
//   float f = 3.14159265358979;            // WRONG
//   int i = * reinterpret_cast<int*>(&f);  // WRONG
//
// The address-casting method actually produces undefined behavior
// according to ISO C++ specification section 3.10 -15 -.  Roughly, this
// section says: if an object in memory has one type, and a program
// accesses it with a different type, then the result is undefined
// behavior for most values of "different type".
//
// This is true for any cast syntax, either *(int*)&f or
// *reinterpret_cast<int*>(&f).  And it is particularly true for
// conversions between integral lvalues and floating-point lvalues.
//
// The purpose of 3.10 -15- is to allow optimizing compilers to assume
// that expressions with different types refer to different memory.  gcc
// 4.0.1 has an optimizer that takes advantage of this.  So a
// non-conforming program quietly produces wildly incorrect output.
//
// The problem is not the use of reinterpret_cast.  The problem is type
// punning: holding an object in memory of one type and reading its bits
// back using a different type.
//
// The C++ standard is more subtle and complex than this, but that
// is the basic idea.
//
// Anyways ...
//
// bit_cast<> calls memcpy() which is blessed by the standard,
// especially by the example in section 3.9 .  Also, of course,
// bit_cast<> wraps up the nasty logic in one place.
//
// Fortunately memcpy() is very fast.  In optimized mode, with a
// constant size, gcc 2.95.3, gcc 4.0.1, and msvc 7.1 produce inline
// code with the minimal amount of data movement.  On a 32-bit system,
// memcpy(d,s,4) compiles to one load and one store, and memcpy(d,s,8)
// compiles to two loads and two stores.
//
// I tested this code with gcc 2.95.3, gcc 4.0.1, icc 8.1, and msvc 7.1.
//
// WARNING: if Dest or Source is a non-POD type, the result of the memcpy
// is likely to surprise you.
// 
// bit_cast<Dest,Source> 是一个实现相当于 "*reinterpret_cast<Dest*>(&source)"
// 的模板函数。我们需要非常低级的功能，如 protobuf 库和快速的数学转换支持
//
// reinterpret_cast 地址转换方法实际上根据 ISO C++ 规范第 3.10-15 段是未定义的行为。
// 大致地说，如果内存中的一个对象有一个类型，并且一个程序以不同的类型访问它，那么结果的
// 值是未定义行为。问题不是 reinterpret_cast 的使用。 问题是类型冲突：将对象保存一种
// 类型，并使用不同的类型读取。
// 
// 按位转换模版函数（警告：如果 Dest 或 Source 是一个 non-POD 类型 ，memcpy 的结果
// 大多数情况是未定义）
// 
// Use like:
// float f = 3.14159265358979;
// int i = bit_cast<int32_t>(f);    // i = 0x40490fdb
namespace butil {
template <class Dest, class Source>
inline Dest bit_cast(const Source& source) {
  COMPILE_ASSERT(sizeof(Dest) == sizeof(Source), VerifySizesAreEqual);

  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}
}  // namespace butil

// Used to explicitly mark the return value of a function as unused. If you are
// really sure you don't want to do anything with the return value of a function
// that has been marked WARN_UNUSED_RESULT, wrap it with this. Example:
//
//   scoped_ptr<MyType> my_var = ...;
//   if (TakeOwnership(my_var.get()) == SUCCESS)
//     ignore_result(my_var.release());
//
// 用于将函数的返回值显式标记为未使用
// 如果确实不想对标记为 WARN_UNUSED_RESULT 的函数的返回值执行任何操作，请将其返回值包装起来
namespace butil {
template<typename T>
inline void ignore_result(const T&) {
}
} // namespace butil

// The following enum should be used only as a constructor argument to indicate
// that the variable has static storage class, and that the constructor should
// do nothing to its state.  It indicates to the reader that it is legal to
// declare a static instance of the class, provided the constructor is given
// the butil::LINKER_INITIALIZED argument.  Normally, it is unsafe to declare a
// static variable that has a constructor or a destructor because invocation
// order is undefined.  However, IF the type can be initialized by filling with
// zeroes (which the loader does for static variables), AND the destructor also
// does nothing to the storage, AND there are no virtual methods, then a
// constructor declared as
//       explicit MyClass(butil::LinkerInitialized x) {}
// and invoked as
//       static MyClass my_variable_name(butil::LINKER_INITIALIZED);
//       
// 以下枚举只能用作构造函数参数，来表示变量具有静态存储类，并且构造函数对其状态不应该执行任何操
// 作。它指出，声明该类的静态实例是合法的，前提是构造函数被赋予了 butil::LINKER_INITIALIZED
// 参数
// 
// @tips
// 通常，由于调用顺序未定义，声明具有构造函数或析构函数的静态变量是不安全的。然而，如果类型可以
// 通过填充零（加载器为静态变量执行）来初始化，并且析构函数对于存储也没有任何作用，并且没有虚方
// 法，则构造函数被声明为:
//      explicit MyClass(butil::LinkerInitialized x) {}
// 则可调用
//     static MyClass my_variable_name(butil::LINKER_INITIALIZED);
// 
namespace butil {
enum LinkerInitialized { LINKER_INITIALIZED };

// Use these to declare and define a static local variable (static T;) so that
// it is leaked so that its destructors are not called at exit. If you need
// thread-safe initialization, use butil/lazy_instance.h instead.
// 
// 使用这些来声明和定义静态局部变量（static T;），会造成内存泄露，以便在退出时不调用析构函数
// 如果您需要线程安全初始化，请使用 butil/lazy_instance.h
#undef CR_DEFINE_STATIC_LOCAL
#define CR_DEFINE_STATIC_LOCAL(type, name, arguments) \
  static type& name = *new type arguments

}  // namespace butil

// Convert symbol to string
// 
// 转换一个 token 符号为字符串
#ifndef BAIDU_SYMBOLSTR
# define BAIDU_SYMBOLSTR(a) BAIDU_SYMBOLSTR_HELPER(a)
# define BAIDU_SYMBOLSTR_HELPER(a) #a
#endif

// 类型推导，C++11 中支持 decltype 关键字
#ifndef BAIDU_TYPEOF
# if defined(BUTIL_CXX11_ENABLED)
#  define BAIDU_TYPEOF decltype
# else
#  ifdef _MSC_VER
#   include <boost/typeof/typeof.hpp>
#   define BAIDU_TYPEOF BOOST_TYPEOF
#  else
#   define BAIDU_TYPEOF typeof
#  endif
# endif // BUTIL_CXX11_ENABLED
#endif  // BAIDU_TYPEOF

// ptr:     the pointer to the member.
// type:    the type of the container struct this is embedded in.
// member:  the name of the member within the struct.
// 
// 通过指向 type 类型结构的成员变量 member 的指针 ptr ，推算出指向结构体 type 的起始指针
// 
// container_of 分为两部分：
// 1. 通过 BAIDU_TYPEOF 定义一个 member 指针类型的指针变量 __mptr ，（即 __mptr 是指
// 向 member 类型的指针），并将 __mptr 赋值为 ptr
// 2. 通过 offsetof 宏计算出 member 在 type 中的偏移，然后用 member 的实际地址 __mptr 
// 减去偏移，得到 type 的起始地址，即指向 type 类型的指针
// 
// @tips
// #define offsetof(type, member) (size_t)&(((type*)0)->member)
// 将地址 0 强制转换为 type 类型的指针，从而定位到 member 在结构体中偏移位置。编译器认为 0 
// 是一个有效的地址，从而认为 0 是 type 指针的起始地址。
#ifndef container_of
# define container_of(ptr, type, member) ({                             \
            const BAIDU_TYPEOF( ((type *)0)->member ) *__mptr = (ptr);  \
            (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

// DEFINE_SMALL_ARRAY(MyType, my_array, size, 64);
//   my_array is typed `MyType*' and as long as `size'. If `size' is not
//   greater than 64, the array is allocated on stack.
//
// NOTE: NEVER use ARRAY_SIZE(my_array) which is always 1.
// 
// 定义小数组局部变量的宏
// DEFINE_SMALL_ARRAY(MyType, my_array, size, 64)
// my_array 是一个 MyType* 类型，元素个数为 size 的变量，如果 size 小于 64，
// my_array 分配在栈上。 ARRAY_SIZE(my_array) 永远返回 1

// RAII 模式，带 delete[] 删除器的数组包装器。 arr 必须是 new[] 申请的资源
#if defined(__cplusplus)
namespace butil {
namespace internal {
template <typename T> struct ArrayDeleter {
    ArrayDeleter() : arr(0) {}
    ~ArrayDeleter() { delete[] arr; }
    T* arr;
};
}}

// Many versions of clang does not support variable-length array with non-pod
// types, have to implement the macro differently.
// 
// 很多 clang 版本不支持 non-pod 类型的变长数组（最低要求：编译器要支持 VLA 特性）
// 
// DEFINE_SMALL_ARRAY(MyType, my_array, size, 64);
// my_array 是一个 MyType* 类型 元素个数为 size 的变量，如果 size 小于 64，
// my_array 分配在栈上。否则分配在堆上。
#if !defined(__clang__)
// 关键点：将堆上申请的内存资源托管给 name##_array_deleter 的 ArrayDeleter<Tp> 对象
// 该 "智能对象" 与数组 name 有同样的作用域、生命周期
# define DEFINE_SMALL_ARRAY(Tp, name, size, maxsize)                    \
    Tp* name = 0;                                                       \
    const unsigned name##_size = (size);                                \
    const unsigned name##_stack_array_size = (name##_size <= (maxsize) ? name##_size : 0); \
    Tp name##_stack_array[name##_stack_array_size];                     \
    ::butil::internal::ArrayDeleter<Tp> name##_array_deleter;            \
    if (name##_stack_array_size) {                                      \
        name = name##_stack_array;                                      \
    } else {                                                            \
        name = new (::std::nothrow) Tp[name##_size];                    \
        name##_array_deleter.arr = name;                                \
    }
#else
// This implementation works for GCC as well, however it needs extra 16 bytes
// for ArrayCtorDtor.
// 
// 此实现也适用于 GCC ，但是 ArrayCtorDtor 需要额外的 16 个字节
namespace butil {
namespace internal {
// RAII 模式，自动 construct/destruct 包装器。raw memory 由 ArrayDeleter 管理。
// 
// @todo
// 应该要考虑 pod 对象无需循环构造/析构，优化性能
template <typename T> struct ArrayCtorDtor {
    ArrayCtorDtor(void* arr, unsigned size) : _arr((T*)arr), _size(size) {
        // 循环在 raw 内存上构造 T 对象 (placement new 调用 T 的构造函数)
        for (unsigned i = 0; i < size; ++i) { new (_arr + i) T; }
    }
    ~ArrayCtorDtor() {
        // 循环调用 T 析构函数
        for (unsigned i = 0; i < _size; ++i) { _arr[i].~T(); }
    }
private:
    T* _arr;
    unsigned _size;
};
}}
// 关键点：对于不支持 non-pod 类型的变长数组编译器，不仅将堆上申请的内存资源托管给 
// name##_array_deleter 的 ArrayDeleter<Tp> 对象，还要将其 Ctor/Dtor 托管给
// name##_array_ctor_dtor 的 ArrayCtorDtor<Tp> 对象。
// 
// 注意：name##_array_ctor_dtor 要定义在 name##_array_deleter 之后！以便于数
// 组自动 GC 时，首先会执行 ArrayCtorDtor 析构，然后在执行 ArrayDeleter 的析构
# define DEFINE_SMALL_ARRAY(Tp, name, size, maxsize)                    \
    Tp* name = 0;                                                       \
    const unsigned name##_size = (size);                                \
    const unsigned name##_stack_array_size = (name##_size <= (maxsize) ? name##_size : 0); \
    char name##_stack_array[sizeof(Tp) * name##_stack_array_size];      \
    ::butil::internal::ArrayDeleter<char> name##_array_deleter;          \
    if (name##_stack_array_size) {                                      \
        name = (Tp*)name##_stack_array;                                 \
    } else {                                                            \
        name = (Tp*)new (::std::nothrow) char[sizeof(Tp) * name##_size];\
        name##_array_deleter.arr = (char*)name;                         \
    }                                                                   \
    const ::butil::internal::ArrayCtorDtor<Tp> name##_array_ctor_dtor(name, name##_size);
#endif // !defined(__clang__)
#endif // defined(__cplusplus)

// Put following code somewhere global to run it before main():
// 
//   BAIDU_GLOBAL_INIT()
//   {
//       ... your code ...
//   }
//
// Your can:
//   * Write any code and access global variables.
//   * Use ASSERT_*.
//   * Have multiple BAIDU_GLOBAL_INIT() in one scope.
// 
// Since the code run in global scope, quit with exit() or similar functions.
// 
// 在 main() 函数执行前的全局初始化代码。在 C++ 中，可以利用声明定义一个全局的结构体变
// 量，来实现该特性（构造函数自动执行）。

#if defined(__cplusplus)
# define BAIDU_GLOBAL_INIT                                      \
namespace {  /*anonymous namespace */                           \
    struct BAIDU_CONCAT(BaiduGlobalInit, __LINE__) {            \
        BAIDU_CONCAT(BaiduGlobalInit, __LINE__)() { init(); }   \
        void init();                                            \
    } BAIDU_CONCAT(baidu_global_init_dummy_, __LINE__);         \
}  /* anonymous namespace */                                    \
    void BAIDU_CONCAT(BaiduGlobalInit, __LINE__)::init              
#else
# define BAIDU_GLOBAL_INIT                      \
    static void __attribute__((constructor))    \
    BAIDU_CONCAT(baidu_global_init_, __LINE__)

#endif  // __cplusplus

#endif  // BUTIL_MACROS_H_

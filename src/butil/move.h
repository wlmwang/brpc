// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_MOVE_H_
#define BUTIL_MOVE_H_

// @tips
// 一、左值、右值
// 1. 在 C/C++ 中，所有的表达式和变量要么是左值，要么是右值。
// 2. 左值指的是既能够出现在等号左边也能出现在等号右边的变量(或表达式)，右值指的则是
//	只能出现在等号右边的变量(或表达式)。
// 3. 通俗的说，左值的定义就是非临时对象：那些可以在多条语句中使用的对象。右值是指临时
// 	的对象，它们只在当前的语句中有效，语句结束后，随即销毁。
// 4. 一般而言，可以取地址、有名字的变量是左值，反之就是右值。
// 5. 在 C 中，右值是不能被修改的。但在 C++ 中，自定义类型的右值是可以被修改的。例如：
// 	T().set().get();
// 6. 特别的： ++/-- ，前缀版本返回左值，后缀版本返回右值。
// 
// 二、左值引用、右值引用
// 1. 左值引用 (&) 这个概念和右值引用 (&&) 是对应的。左值引用就是常规引用，右值引用只
// 	能绑定到一个右值。
// 2. 特别的：在 C++11 以前，右值是不能被引用的。但可以被 const 类型的引用所指向。如：
// 	const int i& = 10;
// 
// i.e.
// 简单应用：（实际上右值引用最主要用途是实现自定义类的资源转移）
// void process_value(int& i) {}	// 1
// void process_value(int&& i) {}	// 2
// int a = 0; 
// process_value(a);	// -> 1
// process_value(1); 	// -> 2
// 
// 三、移动语义
// 右值引用是用来支持转移语义的。
// 转移语义可以将资源 (堆，系统对象等) 从一个对象转移到另一个对象，这样能够减少不必要的
// 	临时对象的创建、拷贝以及销毁，能够大幅度提高 C++ 应用程序的性能。临时对象的维护 (创
// 	建和销毁) 对性能有严重影响。
// 
// 标准库 std::move ，就是 static_cast<T&&> 简单包装，将一个左值转换为右值引用。
// 
// i.e.
// 转发问题：
// void process_value(int& i) {}	// 1
// void process_value(int&& i) {}	// 2
// void forward_value(int&& i) {	// 3
// 		process_value(i);	// -> 1
// }
// int a = 0; 
// process_value(a);	// -> 1
// process_value(1); 	// -> 2
// forward_value(2);	// -> 3
// 虽然 2 这个立即数在函数 forward_value 接收时是右值，但到了 process_value 接收时，
// 变成了左值。
// 
// 可以修改：
// void forward_value(int&& i) {
// 		process_value(std::move(i));	// -> 2
// }
// 
// 四、完美转发
// 将一组参数原封不动的传递给另一个函数。转发过程，除了参数值之外，左值/右值和 const/non-const 
// 也都不能丢失。
// 
// i.e.
// template <typename T> void forward_value(T&& val) {
// 		process_value(val);
// }
// 
// 这得益于， C++11 中定义的 T&& 的推导规则：右值实参为右值引用，左值实参仍然为左值引用。一句话，
// 就是参数的属性不变。 


// Macro with the boilerplate that makes a type move-only in C++03.
//
// USAGE
//
// This macro should be used instead of DISALLOW_COPY_AND_ASSIGN to create
// a "move-only" type.  Unlike DISALLOW_COPY_AND_ASSIGN, this macro should be
// the first line in a class declaration.
//
// A class using this macro must call .Pass() (or somehow be an r-value already)
// before it can be:
//
//   * Passed as a function argument
//   * Used as the right-hand side of an assignment
//   * Returned from a function
//
// Each class will still need to define their own "move constructor" and "move
// operator=" to make this useful.  Here's an example of the macro, the move
// constructor, and the move operator= from the scoped_ptr class:
//
//  template <typename T>
//  class scoped_ptr {
//     MOVE_ONLY_TYPE_FOR_CPP_03(scoped_ptr, RValue)
//   public:
//    scoped_ptr(RValue& other) : ptr_(other.release()) { }
//    scoped_ptr& operator=(RValue& other) {
//      swap(other);
//      return *this;
//    }
//  };
//
// Note that the constructor must NOT be marked explicit.
//
// For consistency, the second parameter to the macro should always be RValue
// unless you have a strong reason to do otherwise.  It is only exposed as a
// macro parameter so that the move constructor and move operator= don't look
// like they're using a phantom type.
//
//
// HOW THIS WORKS
//
// For a thorough explanation of this technique, see:
//
//   http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Move_Constructor
//
// The summary is that we take advantage of 2 properties:
//
//   1) non-const references will not bind to r-values.
//   2) C++ can apply one user-defined conversion when initializing a
//      variable.
//
// The first lets us disable the copy constructor and assignment operator
// by declaring private version of them with a non-const reference parameter.
//
// For l-values, direct initialization still fails like in
// DISALLOW_COPY_AND_ASSIGN because the copy constructor and assignment
// operators are private.
//
// For r-values, the situation is different. The copy constructor and
// assignment operator are not viable due to (1), so we are trying to call
// a non-existent constructor and non-existing operator= rather than a private
// one.  Since we have not committed an error quite yet, we can provide an
// alternate conversion sequence and a constructor.  We add
//
//   * a private struct named "RValue"
//   * a user-defined conversion "operator RValue()"
//   * a "move constructor" and "move operator=" that take the RValue& as
//     their sole parameter.
//
// Only r-values will trigger this sequence and execute our "move constructor"
// or "move operator=."  L-values will match the private copy constructor and
// operator= first giving a "private in this context" error.  This combination
// gives us a move-only type.
//
// For signaling a destructive transfer of data from an l-value, we provide a
// method named Pass() which creates an r-value for the current instance
// triggering the move constructor or move operator=.
//
// Other ways to get r-values is to use the result of an expression like a
// function call.
//
// Here's an example with comments explaining what gets triggered where:
//
//    class Foo {
//      MOVE_ONLY_TYPE_FOR_CPP_03(Foo, RValue);
//
//     public:
//       ... API ...
//       Foo(RValue other);           // Move constructor.
//       Foo& operator=(RValue rhs);  // Move operator=
//    };
//
//    Foo MakeFoo();  // Function that returns a Foo.
//
//    Foo f;
//    Foo f_copy(f);  // ERROR: Foo(Foo&) is private in this context.
//    Foo f_assign;
//    f_assign = f;   // ERROR: operator=(Foo&) is private in this context.
//
//
//    Foo f(MakeFoo());      // R-value so alternate conversion executed.
//    Foo f_copy(f.Pass());  // R-value so alternate conversion executed.
//    f = f_copy.Pass();     // R-value so alternate conversion executed.
//
//
// IMPLEMENTATION SUBTLETIES WITH RValue
//
// The RValue struct is just a container for a pointer back to the original
// object. It should only ever be created as a temporary, and no external
// class should ever declare it or use it in a parameter.
//
// It is tempting to want to use the RValue type in function parameters, but
// excluding the limited usage here for the move constructor and move
// operator=, doing so would mean that the function could take both r-values
// and l-values equially which is unexpected.  See COMPARED To Boost.Move for
// more details.
//
// An alternate, and incorrect, implementation of the RValue class used by
// Boost.Move makes RValue a fieldless child of the move-only type. RValue&
// is then used in place of RValue in the various operators.  The RValue& is
// "created" by doing *reinterpret_cast<RValue*>(this).  This has the appeal
// of never creating a temporary RValue struct even with optimizations
// disabled.  Also, by virtue of inheritance you can treat the RValue
// reference as if it were the move-only type itself.  Unfortunately,
// using the result of this reinterpret_cast<> is actually undefined behavior
// due to C++98 5.2.10.7. In certain compilers (e.g., NaCl) the optimizer
// will generate non-working code.
//
// In optimized builds, both implementations generate the same assembly so we
// choose the one that adheres to the standard.
//
//
// WHY HAVE typedef void MoveOnlyTypeForCPP03
//
// Callback<>/Bind() needs to understand movable-but-not-copyable semantics
// to call .Pass() appropriately when it is expected to transfer the value.
// The cryptic typedef MoveOnlyTypeForCPP03 is added to make this check
// easy and automatic in helper templates for Callback<>/Bind().
// See IsMoveOnlyType template and its usage in butil/callback_internal.h
// for more details.
//
//
// COMPARED TO C++11
//
// In C++11, you would implement this functionality using an r-value reference
// and our .Pass() method would be replaced with a call to std::move().
//
// This emulation also has a deficiency where it uses up the single
// user-defined conversion allowed by C++ during initialization.  This can
// cause problems in some API edge cases.  For instance, in scoped_ptr, it is
// impossible to make a function "void Foo(scoped_ptr<Parent> p)" accept a
// value of type scoped_ptr<Child> even if you add a constructor to
// scoped_ptr<> that would make it look like it should work.  C++11 does not
// have this deficiency.
//
//
// COMPARED TO Boost.Move
//
// Our implementation similar to Boost.Move, but we keep the RValue struct
// private to the move-only type, and we don't use the reinterpret_cast<> hack.
//
// In Boost.Move, RValue is the boost::rv<> template.  This type can be used
// when writing APIs like:
//
//   void MyFunc(boost::rv<Foo>& f)
//
// that can take advantage of rv<> to avoid extra copies of a type.  However you
// would still be able to call this version of MyFunc with an l-value:
//
//   Foo f;
//   MyFunc(f);  // Uh oh, we probably just destroyed |f| w/o calling Pass().
//
// unless someone is very careful to also declare a parallel override like:
//
//   void MyFunc(const Foo& f)
//
// that would catch the l-values first.  This was declared unsafe in C++11 and
// a C++11 compiler will explicitly fail MyFunc(f).  Unfortunately, we cannot
// ensure this in C++03.
//
// Since we have no need for writing such APIs yet, our implementation keeps
// RValue private and uses a .Pass() method to do the conversion instead of
// trying to write a version of "std::move()." Writing an API like std::move()
// would require the RValue struct to be public.
//
//
// CAVEATS
//
// If you include a move-only type as a field inside a class that does not
// explicitly declare a copy constructor, the containing class's implicit
// copy constructor will change from Containing(const Containing&) to
// Containing(Containing&).  This can cause some unexpected errors.
//
//   http://llvm.org/bugs/show_bug.cgi?id=11528
//
// The workaround is to explicitly declare your copy constructor.
//
//
// 应该使用这个宏来代替 DISALLOW_COPY_AND_ASSIGN 来创建一个 "仅限移动" 的类型，
// 与 DISALLOW_COPY_AND_ASSIGN 不同，此宏应该是类声明中的第一行。
//
// 每个类仍然需要定义自己的 "移动构造函数" 和 "移动赋值运算符"，使其移动语义有用：
// 
// Use like:
// 
//    class Foo {
//      MOVE_ONLY_TYPE_FOR_CPP_03(Foo, RValue);
//
//     public:
//       ... API ...
//       Foo(RValue other);           // Move constructor.
//       Foo& operator=(RValue rhs);  // Move operator=
//    };
//
//    Foo MakeFoo();  // Function that returns a Foo.
//
//    Foo f;
//    Foo f_copy(f);  // ERROR: Foo(Foo&) is private in this context.
//    Foo f_assign;
//    f_assign = f;   // ERROR: operator=(Foo&) is private in this context.
//    
//    
//    Foo f(MakeFoo());      // R-value so alternate conversion executed.
//    Foo f_copy(f.Pass());  // R-value so alternate conversion executed.
//    f = f_copy.Pass();     // R-value so alternate conversion executed.
//    
//    // MakeFoo() | f.Pass(); return non-const rvalue
//    // 找不到 Foo(const Foo&)
//    // 找到 operator RValue() 和 Foo(RValue other)
//    // 先调用 operator RValue() 把 MakeFoo()| f.Pass() 的返回值变为 RValue 类型
//    // 调用 Foo(RValue other) 来构造 f | f_copy
//
//
// 该 MOVE_ONLY_TYPE_FOR_CPP_03 宏做了 4 件事情：
// 1. 把 operator= 和 copy 构造（非 const 版本）设置为 private，禁止 copy 和 assign
// 2. 定义了 rvalue_type 类型，该类型包含一个 type* 数据成员
// 3. 实现了 operator rvalue_type 转换函数。通过 rvalue_type 的构造函数把 type 对象
// 	隐式转为一个 rvalue_type 类型对象
// 4. 定义 type Pass() 函数。把 rvalue_type 类型对象显式反转为 type 对象。
// 同时，如果定义了此宏，也必须要求 type 类型要定义一个 type::type(rvalue_type rvalue) 
// 的构造函数。
// 
// 核心思想是：
// 1. 一个普通左值 "拷贝/赋值" 去 "创建/赋值" 一个左值，会调用 type(type&)|operator=(type&) 
// 	被设置为私有，出现编译错误。
// 	如 Foo f; Foo f_copy = f;	// error
// 2. 一个右值 "拷贝/赋值" 去 "创建/赋值" 一个左值，会调用 type(const type&)|operator=(const type&) 
// 	发现都未定义情况下，再去调用 operator rvalue_type(); 以便在调用 type::type(rvalue_type rvalue)| 
// 	operator=(rvalue_type rvalue) 。
// 	如 Foo f; Foo f_move(f.Pass());	// ok
#define MOVE_ONLY_TYPE_FOR_CPP_03(type, rvalue_type) \
 private: \
  struct rvalue_type { \
    explicit rvalue_type(type* object) : object(object) {} \
    type* object; \
  }; \
  type(type&); \
  void operator=(type&); \
 public: \
  operator rvalue_type() { return rvalue_type(this); } \
  type Pass() { return type(rvalue_type(this)); } \
  typedef void MoveOnlyTypeForCPP03; \
 private:

#endif  // BUTIL_MOVE_H_

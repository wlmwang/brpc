// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// scoped_ptr 实现的目标具有 C++11 的 unique_ptr<> 特征的严格子集。已知缺陷包括：
// 不支持 move-only deleteres ，函数指针作为删除器，以及具有引用类型的删除器。
// 
// scoped_refptr 实现的目标具有 C++11 的 shared_ptr<> 特征的严格子集。

// Scopers help you manage ownership of a pointer, helping you easily manage a
// pointer within a scope, and automatically destroying the pointer at the end
// of a scope.  There are two main classes you will use, which correspond to the
// operators new/delete and new[]/delete[].
//
// Example usage (scoped_ptr<T>):
//   {
//     scoped_ptr<Foo> foo(new Foo("wee"));
//   }  // foo goes out of scope, releasing the pointer with it.
//
//   {
//     scoped_ptr<Foo> foo;          // No pointer managed.
//     foo.reset(new Foo("wee"));    // Now a pointer is managed.
//     foo.reset(new Foo("wee2"));   // Foo("wee") was destroyed.
//     foo.reset(new Foo("wee3"));   // Foo("wee2") was destroyed.
//     foo->Method();                // Foo::Method() called.
//     foo.get()->Method();          // Foo::Method() called.
//     SomeFunc(foo.release());      // SomeFunc takes ownership, foo no longer
//                                   // manages a pointer.
//     foo.reset(new Foo("wee4"));   // foo manages a pointer again.
//     foo.reset();                  // Foo("wee4") destroyed, foo no longer
//                                   // manages a pointer.
//   }  // foo wasn't managing a pointer, so nothing was destroyed.
//
// Example usage (scoped_ptr<T[]>):
//   {
//     scoped_ptr<Foo[]> foo(new Foo[100]);
//     foo.get()->Method();  // Foo::Method on the 0th element.
//     foo[10].Method();     // Foo::Method on the 10th element.
//   }
//
// These scopers also implement part of the functionality of C++11 unique_ptr
// in that they are "movable but not copyable."  You can use the scopers in
// the parameter and return types of functions to signify ownership transfer
// in to and out of a function.  When calling a function that has a scoper
// as the argument type, it must be called with the result of an analogous
// scoper's Pass() function or another function that generates a temporary;
// passing by copy will NOT work.  Here is an example using scoped_ptr:
//
//   void TakesOwnership(scoped_ptr<Foo> arg) {
//     // Do something with arg
//   }
//   scoped_ptr<Foo> CreateFoo() {
//     // No need for calling Pass() because we are constructing a temporary
//     // for the return value.
//     return scoped_ptr<Foo>(new Foo("new"));
//   }
//   scoped_ptr<Foo> PassThru(scoped_ptr<Foo> arg) {
//     return arg.Pass();
//   }
//
//   {
//     scoped_ptr<Foo> ptr(new Foo("yay"));  // ptr manages Foo("yay").
//     TakesOwnership(ptr.Pass());           // ptr no longer owns Foo("yay").
//     scoped_ptr<Foo> ptr2 = CreateFoo();   // ptr2 owns the return Foo.
//     scoped_ptr<Foo> ptr3 =                // ptr3 now owns what was in ptr2.
//         PassThru(ptr2.Pass());            // ptr2 is correspondingly NULL.
//   }
//
// Notice that if you do not call Pass() when returning from PassThru(), or
// when invoking TakesOwnership(), the code will not compile because scopers
// are not copyable; they only implement move semantics which require calling
// the Pass() function to signify a destructive transfer of state. CreateFoo()
// is different though because we are constructing a temporary on the return
// line and thus can avoid needing to call Pass().
//
// Pass() properly handles upcast in initialization, i.e. you can use a
// scoped_ptr<Child> to initialize a scoped_ptr<Parent>:
//
//   scoped_ptr<Foo> foo(new Foo());
//   scoped_ptr<FooParent> parent(foo.Pass());
//
// PassAs<>() should be used to upcast return value in return statement:
//
//   scoped_ptr<Foo> CreateFoo() {
//     scoped_ptr<FooChild> result(new FooChild());
//     return result.PassAs<Foo>();
//   }
//
// Note that PassAs<>() is implemented only for scoped_ptr<T>, but not for
// scoped_ptr<T[]>. This is because casting array pointers may not be safe.
// 
// 
// Scopers 可帮助你管理指针的所有权，帮助你轻松管理作用域内的指针，并自动销毁作用域结
// 尾处的指针。你将使用两个主要类，它们对应于运算符 new/delete 和 new[]/delete[]
// 
// Example usage (scoped_ptr<T>):
//   {
//     scoped_ptr<Foo> foo(new Foo("wee"));
//   }  // foo 将自动销毁申请的内存资源
//
//   {
//     scoped_ptr<Foo> foo;          // No pointer managed.
//     foo.reset(new Foo("wee"));    // Now a pointer is managed.
//     foo.reset(new Foo("wee2"));   // Foo("wee") was destroyed.
//     foo.reset(new Foo("wee3"));   // Foo("wee2") was destroyed.
//     foo->Method();                // Foo::Method() called.
//     foo.get()->Method();          // Foo::Method() called.
//     SomeFunc(foo.release());      // SomeFunc takes ownership, foo no longer
//                                   // manages a pointer.  
//                                   // 即，所有权转移给 SomeFunc 函数
//     foo.reset(new Foo("wee4"));   // foo manages a pointer again.
//     foo.reset();                  // Foo("wee4") destroyed, foo no longer
//                                   // manages a pointer.
//   }  // foo wasn't managing a pointer, so nothing was destroyed.
//
// Example usage (scoped_ptr<T[]>):
//   {
//     scoped_ptr<Foo[]> foo(new Foo[100]);
//     foo.get()->Method();  // Foo::Method on the 0th element.
//     foo[10].Method();     // Foo::Method on the 10th element.
//   }
//
//
// scopers 还实现了 C++11 的 unique_ptr 的一部分功能，它们是 "可移动但不可复制的"。
// 你可以使用 scopers 参数或返回的函数，表示所有权转移。当调用具有 scoper 作为参数类
// 型的函数时，必须使用类似的 scoper 的 Pass() 函数或生成临时函数的另一个函数来调用它，
// 通过副本将不起作用。其本质就是把左值变为右值。类似 C++11 里的 std::move()
//
//   void TakesOwnership(scoped_ptr<Foo> arg) {
//     // Do something with arg
//   }
//   scoped_ptr<Foo> CreateFoo() {
//     // No need for calling Pass() because we are constructing a temporary
//     // for the return value.
//     //
//     // 无需调用 Pass() ，因为返回值是临时量（右值）
//     return scoped_ptr<Foo>(new Foo("new"));
//   }
//   scoped_ptr<Foo> PassThru(scoped_ptr<Foo> arg) {
//     // 必须调用 Pass() ，通过副本将不起作用。这里类似 C++11 里的 std::move(arg)
//     // 本质上，因为此时的 arg 是一个左值，要转换成右值才能返回。
//     return arg.Pass();
//   }
//
//   {
//     scoped_ptr<Foo> ptr(new Foo("yay"));  // ptr manages Foo("yay").
//     TakesOwnership(ptr.Pass());           // ptr no longer owns Foo("yay").
//                                           // ptr 所有权转移，TakesOwnership 持有
//     scoped_ptr<Foo> ptr2 = CreateFoo();   // ptr2 owns the return Foo.
//     scoped_ptr<Foo> ptr3 =                // ptr3 now owns what was in ptr2.
//         PassThru(ptr2.Pass());            // ptr2 is correspondingly NULL.
//                                           // ptr2 所有权转让，最后 ptr3 持有
//   }
//
//
// Pass() 在初始化时可以正确处理 upcast 。即：
// 可以使用 scoped_ptr<Child> 初始化 scoped_ptr<Parent>
//
//   scoped_ptr<Foo> foo(new Foo());
//   scoped_ptr<FooParent> parent(foo.Pass());
//
// PassAs<>() 常被用在函数返回值，并且具有 upcast 一层转换。
//
//   scoped_ptr<Foo> CreateFoo() {
//     scoped_ptr<FooChild> result(new FooChild());
//     return result.PassAs<Foo>();
//   }
//
// 请注意，PassAs<>() 仅针对 scoped_ptr<T> 实现，但不适用于 scoped_ptr<T[]>
// 这是因为转换数组指针可能不安全。

#ifndef BUTIL_MEMORY_SCOPED_PTR_H_
#define BUTIL_MEMORY_SCOPED_PTR_H_

// This is an implementation designed to match the anticipated future TR2
// implementation of the scoped_ptr class.

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include <algorithm>  // For std::swap().

#include "butil/basictypes.h"
#include "butil/compiler_specific.h"
#include "butil/move.h"
#include "butil/type_traits.h"

namespace butil {

namespace subtle {
class RefCountedBase;
class RefCountedThreadSafeBase;
}  // namespace subtle

// Function object which deletes its parameter, which must be a pointer.
// If C is an array type, invokes 'delete[]' on the parameter; otherwise,
// invokes 'delete'. The default deleter for scoped_ptr<T>.
// 
// 删除器（函数对象）用来释放其 ptr 参数所指资源，它必须是一个指针。如果 T 是数组类型，
// 则在参数上调用 'delete[]' ，否则，调用 'delete'
// 
// scoped_ptr<T> 的默认删除器
template <class T>
struct DefaultDeleter {
  DefaultDeleter() {}
  template <typename U> DefaultDeleter(const DefaultDeleter<U>&) {
    // IMPLEMENTATION NOTE: C++11 20.7.1.1.2p2 only provides this constructor
    // if U* is implicitly convertible to T* and U is not an array type.
    //
    // Correct implementation should use SFINAE to disable this
    // constructor. However, since there are no other 1-argument constructors,
    // using a COMPILE_ASSERT() based on is_convertible<> and requiring
    // complete types is simpler and will cause compile failures for equivalent
    // misuses.
    //
    // Note, the is_convertible<U*, T*> check also ensures that U is not an
    // array. T is guaranteed to be a non-array, so any U* where U is an array
    // cannot convert to T*.
    // 
    // 要求： U* 能隐式转换为 T* ，并且 U 不是数组类型。 C++11 以前仅提供此构造函数。
    // is_convertible<U*, T*> 也能确保 U 不是数组。
    // T 保证是一个非数组，所以任何 U* 其中 U 是一个数组都不能转换为 T*
    // 
    // 两个 enum 完成 T/U 类型是否是完全类的判别。
    enum { T_must_be_complete = sizeof(T) };
    enum { U_must_be_complete = sizeof(U) };
    COMPILE_ASSERT((butil::is_convertible<U*, T*>::value),
                   U_ptr_must_implicitly_convert_to_T_ptr);
  }
  inline void operator()(T* ptr) const {
    enum { type_must_be_complete = sizeof(T) };
    delete ptr;
  }
};

// Specialization of DefaultDeleter for array types.
// 
// T 为数组类型的偏特化
template <class T>
struct DefaultDeleter<T[]> {
  inline void operator()(T* ptr) const {
    enum { type_must_be_complete = sizeof(T) };
    delete[] ptr;
  }

 private:
  // Disable this operator for any U != T because it is undefined to execute
  // an array delete when the static type of the array mismatches the dynamic
  // type.
  //
  // References:
  //   C++98 [expr.delete]p3
  //   http://cplusplus.github.com/LWG/lwg-defects.html#938
  //   
  // 当数组的静态类型与动态类型不匹配时，执行数组 delete[] 删除是未定义为，所以任何 U != T 
  // 禁用此运算符。
  template <typename U> void operator()(U* array) const;
};

// 数组模板偏特化。如，能匹配类型 scoped_ptr<int[10]>
template <class T, int n>
struct DefaultDeleter<T[n]> {
  // Never allow someone to declare something like scoped_ptr<int[10]>.
  // 
  // 不允许声明 scoped_ptr<int[10]> 类型
  COMPILE_ASSERT(sizeof(T) == -1, do_not_use_array_with_size_as_type);
};

// Function object which invokes 'free' on its parameter, which must be
// a pointer. Can be used to store malloc-allocated pointers in scoped_ptr:
//
// scoped_ptr<int, butil::FreeDeleter> foo_ptr(
//     static_cast<int*>(malloc(sizeof(int))));
//     
// free 其参数 ptr 的内存资源，它必须是一个指针
struct FreeDeleter {
  inline void operator()(void* ptr) const {
    free(ptr);
  }
};

namespace internal {

// 类型 T ，必须继承了一个引用计数器的基类 RefCountedBase/RefCountedThreadSafeBase
template <typename T> struct IsNotRefCounted {
  enum {
    value = !butil::is_convertible<T*, butil::subtle::RefCountedBase*>::value &&
        !butil::is_convertible<T*, butil::subtle::RefCountedThreadSafeBase*>::
            value
  };
};

// Minimal implementation of the core logic of scoped_ptr, suitable for
// reuse in both scoped_ptr and its specializations.
// 
// scoped_ptr 的核心逻辑的最小化实现，适用于 scoped_ptr 及其它专业化中的复用。
template <class T, class D>
class scoped_ptr_impl {
 public:
  explicit scoped_ptr_impl(T* p) : data_(p) { }

  // Initializer for deleters that have data parameters.
  // 
  // 具有数据参数、删除器的初始化
  scoped_ptr_impl(T* p, const D& d) : data_(p, d) {}

  // Templated constructor that destructively takes the value from another
  // scoped_ptr_impl.
  // 
  // 模板化的构造函数，从一个 other(scoped_ptr_impl) 构造对象，释放 other 持有指针的所
  // 有权。实现移动语义。
  template <typename U, typename V>
  scoped_ptr_impl(scoped_ptr_impl<U, V>* other)
      : data_(other->release(), other->get_deleter()) {
    // We do not support move-only deleters.  We could modify our move
    // emulation to have butil::subtle::move() and butil::subtle::forward()
    // functions that are imperfect emulations of their C++11 equivalents,
    // but until there's a requirement, just assume deleters are copyable.
  }

  template <typename U, typename V>
  void TakeState(scoped_ptr_impl<U, V>* other) {
    // See comment in templated constructor above regarding lack of support
    // for move-only deleters.
    // 
    // 释放 other 持有的指针所有权，删除当前对象持有的指针内存。实现移动语义。
    reset(other->release());
    get_deleter() = other->get_deleter();
  }

  ~scoped_ptr_impl() {
    if (data_.ptr != NULL) {
      // Not using get_deleter() saves one function call in non-optimized
      // builds.
      // 
      // 调用删除器函数对象释放内存
      static_cast<D&>(data_)(data_.ptr);
    }
  }

  void reset(T* p) {
    // This is a self-reset, which is no longer allowed: http://crbug.com/162971
    // 
    // 不允许自我重置
    if (p != NULL && p == data_.ptr)
      abort();

    // Note that running data_.ptr = p can lead to undefined behavior if
    // get_deleter()(get()) deletes this. In order to prevent this, reset()
    // should update the stored pointer before deleting its old value.
    //
    // However, changing reset() to use that behavior may cause current code to
    // break in unexpected ways. If the destruction of the owned object
    // dereferences the scoped_ptr when it is destroyed by a call to reset(),
    // then it will incorrectly dispatch calls to |p| rather than the original
    // value of |data_.ptr|.
    //
    // During the transition period, set the stored pointer to NULL while
    // deleting the object. Eventually, this safety check will be removed to
    // prevent the scenario initially described from occuring and
    // http://crbug.com/176091 can be closed.
    // 
    // 重置当前对象持有的指针地址，删除当前对象持有的指针内存。
    T* old = data_.ptr;
    data_.ptr = NULL;
    if (old != NULL)
      static_cast<D&>(data_)(old);
    data_.ptr = p;
  }

  T* get() const { return data_.ptr; }

  // 获取删除器对象。注意：此处有类型转换！！！
  D& get_deleter() { return data_; }
  const D& get_deleter() const { return data_; }

  void swap(scoped_ptr_impl& p2) {
    // Standard swap idiom: 'using std::swap' ensures that std::swap is
    // present in the overload set, but we call swap unqualified so that
    // any more-specific overloads can be used, if available.
    // 
    // 标准交换语法：使用 'std::swap' 会强制使用 std::swap 交换操作。
    // 若不严格限定的调用 swap() 则会有机会可以使用任何更具体的重载（如果可用）
    using std::swap;
    swap(static_cast<D&>(data_), static_cast<D&>(p2.data_));
    swap(data_.ptr, p2.data_.ptr);
  }

  // 释放当前持有指针所有权，作为返回值。以便实现移动语义，即转移所有权。
  T* release() {
    T* old_ptr = data_.ptr;
    data_.ptr = NULL;
    return old_ptr;
  }

 private:
  // Needed to allow type-converting constructor.
  // 
  // 友元声明，可实现任何类型间互相友元访问。
  template <typename U, typename V> friend class scoped_ptr_impl;

  // Use the empty base class optimization to allow us to have a D
  // member, while avoiding any space overhead for it when D is an
  // empty class.  See e.g. http://www.cantrip.org/emptyopt.html for a good
  // discussion of this technique.
  // 
  // 数据、删除器包装器
  struct Data : public D {
    explicit Data(T* ptr_in) : ptr(ptr_in) {}
    Data(T* ptr_in, const D& other) : D(other), ptr(ptr_in) {}
    T* ptr;   // 持有的资源
  };

  Data data_;

  DISALLOW_COPY_AND_ASSIGN(scoped_ptr_impl);
};

}  // namespace internal

}  // namespace butil

// A scoped_ptr<T> is like a T*, except that the destructor of scoped_ptr<T>
// automatically deletes the pointer it holds (if any).
// That is, scoped_ptr<T> owns the T object that it points to.
// Like a T*, a scoped_ptr<T> may hold either NULL or a pointer to a T object.
// Also like T*, scoped_ptr<T> is thread-compatible, and once you
// dereference it, you get the thread safety guarantees of T.
//
// The size of scoped_ptr is small. On most compilers, when using the
// DefaultDeleter, sizeof(scoped_ptr<T>) == sizeof(T*). Custom deleters will
// increase the size proportional to whatever state they need to have. See
// comments inside scoped_ptr_impl<> for details.
//
// Current implementation targets having a strict subset of  C++11's
// unique_ptr<> features. Known deficiencies include not supporting move-only
// deleteres, function pointers as deleters, and deleters with reference
// types.
// 
// scoped_ptr<T> 就像一个 T*，除了 scoped_ptr<T> 的析构函数自动删除它所持有的指针（如
// 果有的话）。也就是说，scoped_ptr<T> 拥有它指向的T对象。像 T* 一样，scoped_ptr<T> 可
// 以保存为 NULL 或指向 T 对象的指针。也可像 T* 使用。 scoped_ptr<T> 是线程兼容的，一旦
// 你 "解引用"，得到的 T 有线程安全保证。
//
// scoped_ptr 大小很小。在大多数编译器中，当使用 DefaultDeleter 时，sizeof(scoped_ptr<T>) 
// == sizeof(T*)
//
// 当前的实现目标具有 C++11 的 unique_ptr<> 特征的严格子集。
// 已知缺陷包括：不支持 move-only deleteres ，函数指针作为删除器，以及具有引用类型的删除器。
template <class T, class D = butil::DefaultDeleter<T> >
class scoped_ptr {
  MOVE_ONLY_TYPE_FOR_CPP_03(scoped_ptr, RValue)

  // 类型 T ，必须继承了一个引用计数器的基类 RefCountedBase/RefCountedThreadSafeBase
  COMPILE_ASSERT(butil::internal::IsNotRefCounted<T>::value,
                 T_is_refcounted_type_and_needs_scoped_refptr);

 public:
  // The element and deleter types.
  typedef T element_type;
  typedef D deleter_type;

  // Constructor.  Defaults to initializing with NULL.
  scoped_ptr() : impl_(NULL) { }

  // Constructor.  Takes ownership of p.
  explicit scoped_ptr(element_type* p) : impl_(p) { }

  // Constructor.  Allows initialization of a stateful deleter.
  scoped_ptr(element_type* p, const D& d) : impl_(p, d) { }

  // Constructor.  Allows construction from a scoped_ptr rvalue for a
  // convertible type and deleter.
  //
  // IMPLEMENTATION NOTE: C++11 unique_ptr<> keeps this constructor distinct
  // from the normal move constructor. By C++11 20.7.1.2.1.21, this constructor
  // has different post-conditions if D is a reference type. Since this
  // implementation does not support deleters with reference type,
  // we do not need a separate move constructor allowing us to avoid one
  // use of SFINAE. You only need to care about this if you modify the
  // implementation of scoped_ptr.
  // 
  // 转移指针所有权。 std::unique_ptr 是禁止本行为的。
  // 有点类似 auto_ptr 构造行为， 当然同类对象不能构造另一个对象。
  template <typename U, typename V>
  scoped_ptr(scoped_ptr<U, V> other) : impl_(&other.impl_) {
    COMPILE_ASSERT(!butil::is_array<U>::value, U_cannot_be_an_array);
  }

  // Constructor.  Move constructor for C++03 move emulation of this type.
  // 
  // 移动构造函数。
  scoped_ptr(RValue rvalue) : impl_(&rvalue.object->impl_) { }

  // operator=.  Allows assignment from a scoped_ptr rvalue for a convertible
  // type and deleter.
  //
  // IMPLEMENTATION NOTE: C++11 unique_ptr<> keeps this operator= distinct from
  // the normal move assignment operator. By C++11 20.7.1.2.3.4, this templated
  // form has different requirements on for move-only Deleters. Since this
  // implementation does not support move-only Deleters, we do not need a
  // separate move assignment operator allowing us to avoid one use of SFINAE.
  // You only need to care about this if you modify the implementation of
  // scoped_ptr.
  // 
  // 转移指针所有权。 std::unique_ptr 是禁止本行为的。
  // 有点类似 auto_ptr 赋值行为，当然同类对象不能赋值给另一个对象。
  template <typename U, typename V>
  scoped_ptr& operator=(scoped_ptr<U, V> rhs) {
    COMPILE_ASSERT(!butil::is_array<U>::value, U_cannot_be_an_array);
    impl_.TakeState(&rhs.impl_);
    return *this;
  }

  // Reset.  Deletes the currently owned object, if any.
  // Then takes ownership of a new object, if given.
  // 
  // 重置指针，释放当前对象持有指针指向的内存。
  void reset(element_type* p = NULL) { impl_.reset(p); }

  // Accessors to get the owned object.
  // operator* and operator-> will assert() if there is no current object.
  // 
  // 指针运算符重载
  element_type& operator*() const {
    assert(impl_.get() != NULL);
    return *impl_.get();
  }
  element_type* operator->() const  {
    assert(impl_.get() != NULL);
    return impl_.get();
  }
  element_type* get() const { return impl_.get(); }

  // Access to the deleter.
  // 
  // 访问删除器
  deleter_type& get_deleter() { return impl_.get_deleter(); }
  const deleter_type& get_deleter() const { return impl_.get_deleter(); }

  // Allow scoped_ptr<element_type> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
  //
  // Note that this trick is only safe when the == and != operators
  // are declared explicitly, as otherwise "scoped_ptr1 ==
  // scoped_ptr2" will compile but do the wrong thing (i.e., convert
  // to Testable and then do the comparison).
  // 
  // 允许在布尔表达式中使用 scoped_ptr<element_type> ，但不能隐式转换为真正的 bool（这
  // 是危险的）。
  // 
  // 当 == 和 != 运算符被显式声明时，这个技巧是安全的。否则 scoped_ptr1 == scoped_ptr2 
  // 仍旧会被编译。但也是错的（即，意外的被转换为 Testable ，然后进行比较）。
  // 
  // 
  // Testable 为 scoped_ptr 的成员变量指针类型，指向 scoped_ptr_impl<> 。
 private:
  typedef butil::internal::scoped_ptr_impl<element_type, deleter_type>
      scoped_ptr::*Testable;

 public:
  // 当前 scoped_ptr 持有指针，返回 scoped_ptr 底层实际包装指针对象指针。
  operator Testable() const { return impl_.get() ? &scoped_ptr::impl_ : NULL; }

  // Comparison operators.
  // These return whether two scoped_ptr refer to the same object, not just to
  // two different but equal objects.
  // 
  // 重载指针之间的比较运算符。
  bool operator==(const element_type* p) const { return impl_.get() == p; }
  bool operator!=(const element_type* p) const { return impl_.get() != p; }

  // Swap two scoped pointers.
  // 
  // 交换操作
  void swap(scoped_ptr& p2) {
    impl_.swap(p2.impl_);
  }

  // Release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  // 
  // 释放当前指针指向的所有权。
  element_type* release() WARN_UNUSED_RESULT {
    return impl_.release();
  }

  // C++98 doesn't support functions templates with default parameters which
  // makes it hard to write a PassAs() that understands converting the deleter
  // while preserving simple calling semantics.
  //
  // Until there is a use case for PassAs() with custom deleters, just ignore
  // the custom deleter.
  // 
  // PassAs<>() 常被用在函数返回值，并且具有 upcast 一层转换。
  template <typename PassAsType>
  scoped_ptr<PassAsType> PassAs() {
    return scoped_ptr<PassAsType>(Pass());
  }

 private:
  // Needed to reach into |impl_| in the constructor.
  // 
  // 友元声明，可实现任何类型间互相友元访问（复制构造使用了 other.impl_）
  template <typename U, typename V> friend class scoped_ptr;
  // 底层资源管理实现对象
  butil::internal::scoped_ptr_impl<element_type, deleter_type> impl_;

  // Forbidden for API compatibility with std::unique_ptr.
  // 
  // 兼容 std::unique_ptr
  explicit scoped_ptr(int disallow_construction_from_null);

  // Forbid comparison of scoped_ptr types.  If U != T, it totally
  // doesn't make sense, and if U == T, it still doesn't make sense
  // because you should never have the same object owned by two different
  // scoped_ptrs.
  // 
  // 禁止 scoped_ptr 之间的比较逻辑运算符。
  // 如果 U != T ，它完全没有意义。
  // 如果 U == T ，它仍然没有意义，因为你不应该拥有两个不同的 scoped_ptrs 拥有的相同对象。
  template <class U> bool operator==(scoped_ptr<U> const& p2) const;
  template <class U> bool operator!=(scoped_ptr<U> const& p2) const;
};

// scoped_ptr 模版的数组的特化版本
template <class T, class D>
class scoped_ptr<T[], D> {
  MOVE_ONLY_TYPE_FOR_CPP_03(scoped_ptr, RValue)

 public:
  // The element and deleter types.
  typedef T element_type;
  typedef D deleter_type;

  // Constructor.  Defaults to initializing with NULL.
  scoped_ptr() : impl_(NULL) { }

  // Constructor. Stores the given array. Note that the argument's type
  // must exactly match T*. In particular:
  // - it cannot be a pointer to a type derived from T, because it is
  //   inherently unsafe in the general case to access an array through a
  //   pointer whose dynamic type does not match its static type (eg., if
  //   T and the derived types had different sizes access would be
  //   incorrectly calculated). Deletion is also always undefined
  //   (C++98 [expr.delete]p3). If you're doing this, fix your code.
  // - it cannot be NULL, because NULL is an integral expression, not a
  //   pointer to T. Use the no-argument version instead of explicitly
  //   passing NULL.
  // - it cannot be const-qualified differently from T per unique_ptr spec
  //   (http://cplusplus.github.com/LWG/lwg-active.html#2118). Users wanting
  //   to work around this may use implicit_cast<const T*>().
  //   However, because of the first bullet in this comment, users MUST
  //   NOT use implicit_cast<Base*>() to upcast the static type of the array.
  explicit scoped_ptr(element_type* array) : impl_(array) { }

  // Constructor.  Move constructor for C++03 move emulation of this type.
  scoped_ptr(RValue rvalue) : impl_(&rvalue.object->impl_) { }

  // operator=.  Move operator= for C++03 move emulation of this type.
  scoped_ptr& operator=(RValue rhs) {
    impl_.TakeState(&rhs.object->impl_);
    return *this;
  }

  // Reset.  Deletes the currently owned array, if any.
  // Then takes ownership of a new object, if given.
  void reset(element_type* array = NULL) { impl_.reset(array); }

  // Accessors to get the owned array.
  element_type& operator[](size_t i) const {
    assert(impl_.get() != NULL);
    return impl_.get()[i];
  }
  element_type* get() const { return impl_.get(); }

  // Access to the deleter.
  deleter_type& get_deleter() { return impl_.get_deleter(); }
  const deleter_type& get_deleter() const { return impl_.get_deleter(); }

  // Allow scoped_ptr<element_type> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
 private:
  typedef butil::internal::scoped_ptr_impl<element_type, deleter_type>
      scoped_ptr::*Testable;

 public:
  operator Testable() const { return impl_.get() ? &scoped_ptr::impl_ : NULL; }

  // Comparison operators.
  // These return whether two scoped_ptr refer to the same object, not just to
  // two different but equal objects.
  bool operator==(element_type* array) const { return impl_.get() == array; }
  bool operator!=(element_type* array) const { return impl_.get() != array; }

  // Swap two scoped pointers.
  void swap(scoped_ptr& p2) {
    impl_.swap(p2.impl_);
  }

  // Release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  element_type* release() WARN_UNUSED_RESULT {
    return impl_.release();
  }

 private:
  // Force element_type to be a complete type.
  enum { type_must_be_complete = sizeof(element_type) };

  // Actually hold the data.
  butil::internal::scoped_ptr_impl<element_type, deleter_type> impl_;

  // Disable initialization from any type other than element_type*, by
  // providing a constructor that matches such an initialization, but is
  // private and has no definition. This is disabled because it is not safe to
  // call delete[] on an array whose static type does not match its dynamic
  // type.
  template <typename U> explicit scoped_ptr(U* array);
  explicit scoped_ptr(int disallow_construction_from_null);

  // Disable reset() from any type other than element_type*, for the same
  // reasons as the constructor above.
  template <typename U> void reset(U* array);
  void reset(int disallow_reset_from_null);

  // Forbid comparison of scoped_ptr types.  If U != T, it totally
  // doesn't make sense, and if U == T, it still doesn't make sense
  // because you should never have the same object owned by two different
  // scoped_ptrs.
  template <class U> bool operator==(scoped_ptr<U> const& p2) const;
  template <class U> bool operator!=(scoped_ptr<U> const& p2) const;
};

// Free functions
template <class T, class D>
void swap(scoped_ptr<T, D>& p1, scoped_ptr<T, D>& p2) {
  p1.swap(p2);
}

template <class T, class D>
bool operator==(T* p1, const scoped_ptr<T, D>& p2) {
  return p1 == p2.get();
}

template <class T, class D>
bool operator!=(T* p1, const scoped_ptr<T, D>& p2) {
  return p1 != p2.get();
}

// A function to convert T* into scoped_ptr<T>
// Doing e.g. make_scoped_ptr(new FooBarBaz<type>(arg)) is a shorter notation
// for scoped_ptr<FooBarBaz<type> >(new FooBarBaz<type>(arg))
// 
// 用于创建一个 scoped_ptr<T> ，不需要输入所有的模板参数（利用函数模板自动推导特性）
template <typename T>
scoped_ptr<T> make_scoped_ptr(T* ptr) {
  return scoped_ptr<T>(ptr);
}

#endif  // BUTIL_MEMORY_SCOPED_PTR_H_

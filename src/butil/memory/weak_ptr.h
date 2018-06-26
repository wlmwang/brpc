// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Weak pointers are pointers to an object that do not affect its lifetime,
// and which may be invalidated (i.e. reset to NULL) by the object, or its
// owner, at any time, most commonly when the object is about to be deleted.

// Weak pointers are useful when an object needs to be accessed safely by one
// or more objects other than its owner, and those callers can cope with the
// object vanishing and e.g. tasks posted to it being silently dropped.
// Reference-counting such an object would complicate the ownership graph and
// make it harder to reason about the object's lifetime.

// EXAMPLE:
//
//  class Controller {
//   public:
//    void SpawnWorker() { Worker::StartNew(weak_factory_.GetWeakPtr()); }
//    void WorkComplete(const Result& result) { ... }
//   private:
//    // Member variables should appear before the WeakPtrFactory, to ensure
//    // that any WeakPtrs to Controller are invalidated before its members
//    // variable's destructors are executed, rendering them invalid.
//    WeakPtrFactory<Controller> weak_factory_;
//  };
//
//  class Worker {
//   public:
//    static void StartNew(const WeakPtr<Controller>& controller) {
//      Worker* worker = new Worker(controller);
//      // Kick off asynchronous processing...
//    }
//   private:
//    Worker(const WeakPtr<Controller>& controller)
//        : controller_(controller) {}
//    void DidCompleteAsynchronousProcessing(const Result& result) {
//      if (controller_)
//        controller_->WorkComplete(result);
//    }
//    WeakPtr<Controller> controller_;
//  };
//
// With this implementation a caller may use SpawnWorker() to dispatch multiple
// Workers and subsequently delete the Controller, without waiting for all
// Workers to have completed.

// ------------------------- IMPORTANT: Thread-safety -------------------------

// Weak pointers may be passed safely between threads, but must always be
// dereferenced and invalidated on the same thread otherwise checking the
// pointer would be racey.
//
// To ensure correct use, the first time a WeakPtr issued by a WeakPtrFactory
// is dereferenced, the factory and its WeakPtrs become bound to the calling
// thread, and cannot be dereferenced or invalidated on any other thread. Bound
// WeakPtrs can still be handed off to other threads, e.g. to use to post tasks
// back to object on the bound thread.
//
// Invalidating the factory's WeakPtrs un-binds it from the thread, allowing it
// to be passed for a different thread to use or delete it.
// 
// 
// 弱引用智能指针是指向对象的指针，其不影响对象生命周期。并且可能在任何时候被对象或其所有者
// 回收（即重置为 NULL ），最常见的是当该所有者被删除。
// 
// 当对象需要被其所有者以外的一个或多个对象安全访问时，弱指针是有用的，并且那些调用者可以处
// 理消失的对象。引用计数这样一个对象会使所有权复杂化，并使得对象的生命周期进行难以推断。
// 
// EXAMPLE:
//
//  class Controller {
//   public:
//    void SpawnWorker() { Worker::StartNew(weak_factory_.GetWeakPtr()); }
//    void WorkComplete(const Result& result) { ... }
//   private:
//    // Member variables should appear before the WeakPtrFactory, to ensure
//    // that any WeakPtrs to Controller are invalidated before its members
//    // variable's destructors are executed, rendering them invalid.
//    WeakPtrFactory<Controller> weak_factory_;   // 创建弱引用智能指针
//  };
//
//  class Worker {
//   public:
//    static void StartNew(const WeakPtr<Controller>& controller) {
//      Worker* worker = new Worker(controller);
//      // Kick off asynchronous processing...
//    }
//   private:
//    Worker(const WeakPtr<Controller>& controller)
//        : controller_(controller) {}
//    void DidCompleteAsynchronousProcessing(const Result& result) {
//      if (controller_)
//        controller_->WorkComplete(result);
//    }
//    WeakPtr<Controller> controller_;  // 弱引用智能指针
//  };
//
// 弱指针可以在线程之间安全地传递，但是必须始终在同一线程上解引用和释放，否则检查指针有竞争。
// 
// 为了确保正确的使用， WeakPtrFactory 产生 WeakPtr 第一次是被解引用的，工厂及其 WeakPtrs 
// 将被绑定到调用线程，并且不能在任何其他线程上被解除引用或释放。绑定的 WeakPtrs 仍然可以
// 交给其他线程，例如：用于将任务发回绑定线程上的对象。

#ifndef BUTIL_MEMORY_WEAK_PTR_H_
#define BUTIL_MEMORY_WEAK_PTR_H_

#include "butil/basictypes.h"
#include "butil/base_export.h"
#include "butil/logging.h"
#include "butil/memory/ref_counted.h"
#include "butil/type_traits.h"

namespace butil {

template <typename T> class SupportsWeakPtr;
template <typename T> class WeakPtr;

namespace internal {
// These classes are part of the WeakPtr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

// 弱引用智能指针（不更新实际指针指向的数据的引用计数器。提供指向数据 is_valid() 是否有效
// 的判定）
class BUTIL_EXPORT WeakReference {
 public:
  // Although Flag is bound to a specific thread, it may be deleted from another
  // via butil::WeakPtr::~WeakPtr().
  // 
  // 虽然 Flag 是绑定到一个特定的线程，它还是可能会被另一个线程通过 
  // butil::WeakPtr::~WeakPtr() 删除。
  // 
  // 一个 Flag 引用计数器。主要用于弱引用智能指针指向的数据是否有效判定。
  class BUTIL_EXPORT Flag : public RefCountedThreadSafe<Flag> {
   public:
    Flag();

    // 设置为"非法"值
    void Invalidate();
    // 是否为"非法"值
    bool IsValid() const;

   private:
    // 用于访问私有的 ～Flag()
    friend class butil::RefCountedThreadSafe<Flag>;

    ~Flag();

    bool is_valid_;
  };

  WeakReference();
  // 此时 flag_ 引用计数器 +1 ，即 flag_::ref_count_>=1
  explicit WeakReference(const Flag* flag);
  ~WeakReference();

  // 持有的 WeakReference::Flag 资源有效
  bool is_valid() const;

 private:
  // 持有 WeakReference::Flag 智能指针（共享型 shared_ptr<>）
  scoped_refptr<const Flag> flag_;
};

// 弱引用所有者智能指针（进行了弱引用智能指针是否是唯一持有者判定。提供获取弱引用智能指针接口）
class BUTIL_EXPORT WeakReferenceOwner {
 public:
  WeakReferenceOwner();
  ~WeakReferenceOwner();

  // 如果我们持有对 WeakReference::Flag 的最后一个引用，则创建一个 WeakReference
  WeakReference GetRef() const;

  // 是否多个智能指针（scoped_refptr<>）在引用 WeakReference::Flag
  bool HasRefs() const {
    return flag_.get() && !flag_->HasOneRef();
  }

  // 设置为"非法"值
  void Invalidate();

 private:
  // 持有 WeakReference::Flag 智能指针（共享型 shared_ptr<>）
  mutable scoped_refptr<WeakReference::Flag> flag_;
};

// This class simplifies the implementation of WeakPtr's type conversion
// constructor by avoiding the need for a public accessor for ref_.  A
// WeakPtr<T> cannot access the private members of WeakPtr<U>, so this
// base class gives us a way to access ref_ in a protected fashion.
// 
// 该类简化了 WeakPtr 类型转换构造函数的实现，避免了对 ref_ 的公共访问器的需要。
// WeakPtr<T> 不能访问 WeakPtr<U> 的私有成员，所以这个基类为我们提供了一种以受
// 保护的方式访问 ref_ 的方式。
class BUTIL_EXPORT WeakPtrBase {
 public:
  WeakPtrBase();
  ~WeakPtrBase();

 protected:
  // 使用 WeakReference 的默认复制构造函数
  explicit WeakPtrBase(const WeakReference& ref);

  WeakReference ref_;
};

// This class provides a common implementation of common functions that would
// otherwise get instantiated separately for each distinct instantiation of
// SupportsWeakPtr<>.
// 
// 这个类提供了常见的函数的通用实现，否则需为 SupportsWeakPtr<> 的每个不同实例分别实
// 例化。
class SupportsWeakPtrBase {
 public:
  // A safe static downcast of a WeakPtr<Base> to WeakPtr<Derived>. This
  // conversion will only compile if there is exists a Base which inherits
  // from SupportsWeakPtr<Base>. See butil::AsWeakPtr() below for a helper
  // function that makes calling this easier.
  // 
  // WeakPtr<Base> 到 WeakPtr<Derived> 的安全静态 downcast 。此转换将仅进行编译期
  // 进行，即要存在从 SupportsWeakPtr<Base> 继承的派生类 Base
  // 请参阅 butil::AsWeakPtr() 下面的一个辅助函数，使调用更容易。
  template<typename Derived>
  static WeakPtr<Derived> StaticAsWeakPtr(Derived* t) {
    // Derived 必须是继承自 SupportsWeakPtrBase 派生类。
    typedef
        is_convertible<Derived, internal::SupportsWeakPtrBase&> convertible;
    COMPILE_ASSERT(convertible::value,
                   AsWeakPtr_argument_inherits_from_SupportsWeakPtr);
    return AsWeakPtrImpl<Derived>(t, *t);
  }

 private:
  // This template function uses type inference to find a Base of Derived
  // which is an instance of SupportsWeakPtr<Base>. We can then safely
  // static_cast the Base* to a Derived*.
  // 
  // 此模板函数使用类型推断来查找 Derived 的 Base ，它是 SupportsWeakPtr<Base> 的
  // 一个实例。我们可以安全地 static_cast Base* 到 Derived*
  template <typename Derived, typename Base>
  static WeakPtr<Derived> AsWeakPtrImpl(
      Derived* t, const SupportsWeakPtr<Base>&) {
    WeakPtr<Base> ptr = t->Base::AsWeakPtr();
    return WeakPtr<Derived>(ptr.ref_, static_cast<Derived*>(ptr.ptr_));
  }
};

}  // namespace internal

template <typename T> class WeakPtrFactory;

// The WeakPtr class holds a weak reference to |T*|.
//
// This class is designed to be used like a normal pointer.  You should always
// null-test an object of this class before using it or invoking a method that
// may result in the underlying object being destroyed.
//
// EXAMPLE:
//
//   class Foo { ... };
//   WeakPtr<Foo> foo;
//   if (foo)
//     foo->method();
//
//
// 该类被设计为像普通指针一样使用。在使用它之前，应该始终对该类的对象进行 null-test 测试，
// 或调用可能导致基础对象被销毁的方法。
//
// EXAMPLE:
//
//   class Foo { ... };
//   WeakPtr<Foo> foo;
//   if (foo)
//     foo->method();
//
//
// WeakPtr 类持有对 |T*| 的弱引用智能指针。
template <typename T>
class WeakPtr : public internal::WeakPtrBase {
 public:
  WeakPtr() : ptr_(NULL) {
  }

  // Allow conversion from U to T provided U "is a" T. Note that this
  // is separate from the (implicit) copy constructor.
  // 
  // 如果 U 公有继承 T，则允许从 U 到 T 的转换（向上转换）。请注意，这与（隐式）复制构造函数
  // 是分开的。
  template <typename U>
  WeakPtr(const WeakPtr<U>& other) : WeakPtrBase(other), ptr_(other.ptr_) {
  }

  // 智能指针是否有效（持有资源）
  T* get() const { return ref_.is_valid() ? ptr_ : NULL; }

  // 解引用操作符重载
  T& operator*() const {
    DCHECK(get() != NULL);
    return *get();
  }
  // 成员访问指针操作符重载
  T* operator->() const {
    DCHECK(get() != NULL);
    return get();
  }

  // Allow WeakPtr<element_type> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
  //
  // Note that this trick is only safe when the == and != operators
  // are declared explicitly, as otherwise "weak_ptr1 == weak_ptr2"
  // will compile but do the wrong thing (i.e., convert to Testable
  // and then do the comparison).
  // 
  // 允许在布尔表达式中使用 WeakPtr<element_type> ，但不能隐式转换为真正的 bool（这
  // 是危险的）。
  // 
  // 当 == 和 != 运算符被显式声明时，这个技巧是安全的。否则 weak_ptr1 == weak_ptr2 
  // 仍旧会被编译。但也是错的（即，意外的被转换为 Testable ，然后进行比较）。
  // 
  // 
  // Testable 为 WeakPtr 的成员变量指针类型，指向 T 类型数据
 private:
  typedef T* WeakPtr::*Testable;

 public:
  operator Testable() const { return get() ? &WeakPtr::ptr_ : NULL; }

  // 重置为空
  void reset() {
    ref_ = internal::WeakReference();
    ptr_ = NULL;
  }

 private:
  // Explicitly declare comparison operators as required by the bool
  // trick, but keep them private.
  // 
  // 根据 bool 技巧的要求显式声明比较运算符，但保持私有
  template <class U> bool operator==(WeakPtr<U> const&) const;
  template <class U> bool operator!=(WeakPtr<U> const&) const;

  friend class internal::SupportsWeakPtrBase;
  template <typename U> friend class WeakPtr;
  friend class SupportsWeakPtr<T>;
  friend class WeakPtrFactory<T>;

  WeakPtr(const internal::WeakReference& ref, T* ptr)
      : WeakPtrBase(ref),
        ptr_(ptr) {
  }

  // This pointer is only valid when ref_.is_valid() is true.  Otherwise, its
  // value is undefined (as opposed to NULL).
  // 
  // 该指针仅在 ref_.is_valid()==true 时有效。否则，它的值是未定义的（与 NULL 相反）
  T* ptr_;
};

// A class may be composed of a WeakPtrFactory and thereby
// control how it exposes weak pointers to itself.  This is helpful if you only
// need weak pointers within the implementation of a class.  This class is also
// useful when working with primitive types.  For example, you could have a
// WeakPtrFactory<bool> that is used to pass around a weak reference to a bool.
// 
// 一个类可以由 WeakPtrFactory 组成，从而控制它如何暴露自身的弱引用智能指针。
// 如果你只需要在类的实现中使用弱引用智能指针，这是有帮助的。当使用原始类型时，此类也很有用。
// 例如，你可以使用一个 WeakPtrFactory<bool> 来传递一个弱引用智能指针到 bool
template <class T>
class WeakPtrFactory {
 public:
  explicit WeakPtrFactory(T* ptr) : ptr_(ptr) {
  }

  ~WeakPtrFactory() {
    ptr_ = NULL;
  }

  WeakPtr<T> GetWeakPtr() {
    DCHECK(ptr_);
    return WeakPtr<T>(weak_reference_owner_.GetRef(), ptr_);
  }

  // Call this method to invalidate all existing weak pointers.
  // 
  // 调用此方法使所有现有的弱引用智能指针失效。
  void InvalidateWeakPtrs() {
    DCHECK(ptr_);
    weak_reference_owner_.Invalidate();
  }

  // Call this method to determine if any weak pointers exist.
  // 
  // 调用此方法来确定是否存在弱引用智能指针。
  bool HasWeakPtrs() const {
    DCHECK(ptr_);
    return weak_reference_owner_.HasRefs();
  }

 private:
  internal::WeakReferenceOwner weak_reference_owner_;
  T* ptr_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(WeakPtrFactory);
};

// A class may extend from SupportsWeakPtr to let others take weak pointers to
// it. This avoids the class itself implementing boilerplate to dispense weak
// pointers.  However, since SupportsWeakPtr's destructor won't invalidate
// weak pointers to the class until after the derived class' members have been
// destroyed, its use can lead to subtle use-after-destroy issues.
// 
// 一个类可以从 SupportsWeakPtr 扩展，让其他人指向它的弱指针。这避免了类本身实现模板来
// 分配弱指针。然而，由于 SupportsWeakPtr 的析构函数不会使弱指针类失效，直到派生类的成
// 员被销毁为止，所以它的使用可能会导致微妙的使用销毁后的问题。
template <class T>
class SupportsWeakPtr : public internal::SupportsWeakPtrBase {
 public:
  SupportsWeakPtr() {}

  WeakPtr<T> AsWeakPtr() {
    return WeakPtr<T>(weak_reference_owner_.GetRef(), static_cast<T*>(this));
  }

 protected:
  ~SupportsWeakPtr() {}

 private:
  internal::WeakReferenceOwner weak_reference_owner_;
  DISALLOW_COPY_AND_ASSIGN(SupportsWeakPtr);
};

// Helper function that uses type deduction to safely return a WeakPtr<Derived>
// when Derived doesn't directly extend SupportsWeakPtr<Derived>, instead it
// extends a Base that extends SupportsWeakPtr<Base>.
//
// EXAMPLE:
//   class Base : public butil::SupportsWeakPtr<Producer> {};
//   class Derived : public Base {};
//
//   Derived derived;
//   butil::WeakPtr<Derived> ptr = butil::AsWeakPtr(&derived);
//
// Note that the following doesn't work (invalid type conversion) since
// Derived::AsWeakPtr() is WeakPtr<Base> SupportsWeakPtr<Base>::AsWeakPtr(),
// and there's no way to safely cast WeakPtr<Base> to WeakPtr<Derived> at
// the caller.
//
//   butil::WeakPtr<Derived> ptr = derived.AsWeakPtr();  // Fails.
//   
// 当 Derived 不直接扩展 SupportsWeakPtr<Derived> ，而是它扩展了已经扩展了 
// SupportsWeakPtr<Base> 的 Base 时，该辅助函数，利用类型推导安全的返回 
// WeakPtr<Derived> 类型。
// 
// EXAMPLE:
//   class Base : public butil::SupportsWeakPtr<Producer> {};
//   class Derived : public Base {};
//
//   Derived derived;
//   butil::WeakPtr<Derived> ptr = butil::AsWeakPtr(&derived);

template <typename Derived>
WeakPtr<Derived> AsWeakPtr(Derived* t) {
  return internal::SupportsWeakPtrBase::StaticAsWeakPtr<Derived>(t);
}

}  // namespace butil

#endif  // BUTIL_MEMORY_WEAK_PTR_H_

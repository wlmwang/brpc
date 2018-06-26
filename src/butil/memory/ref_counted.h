// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 侵入式引用计数器设计（类对象自身持有引用计数器属性）。即，让需要使用引用计数器的类
// 继承自 RefCounted<T> 模板类，再配合使用 scoped_refptr<T> 智能指针进行使用。
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//   };
//   
//   void some_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // 当 some_function 函数返回， |foo| 会被自动析构
//   }
// 
// scoped_refptr 实现的目标具有 C++11 的 shared_ptr<> 特征的严格子集。
// scoped_ptr 实现的目标具有 C++11 的 unique_ptr<> 特征的严格子集。

#ifndef BUTIL_MEMORY_REF_COUNTED_H_
#define BUTIL_MEMORY_REF_COUNTED_H_

#include <cassert>

#include "butil/atomic_ref_count.h"
#include "butil/base_export.h"
#include "butil/compiler_specific.h"
#ifndef NDEBUG
#include "butil/logging.h"
#endif
#include "butil/threading/thread_collision_warner.h"

namespace butil {

namespace subtle {

// 引用计数器基类
class BUTIL_EXPORT RefCountedBase {
 public:
  // 是否只有一个引用计数器
  bool HasOneRef() const { return ref_count_ == 1; }

 protected:
  RefCountedBase()
      : ref_count_(0)
      , in_dtor_(false)
      {
  }

  ~RefCountedBase() {
  #ifndef NDEBUG
    DCHECK(in_dtor_) << "RefCounted object deleted without calling Release()";
  #endif
  }


  // 增加引用计数器
  void AddRef() const {
    // TODO(maruel): Add back once it doesn't assert 500 times/sec.
    // Current thread books the critical section "AddRelease"
    // without release it.
    // DFAKE_SCOPED_LOCK_THREAD_LOCKED(add_release_);
  #ifndef NDEBUG
    DCHECK(!in_dtor_);
  #endif
    ++ref_count_;
  }

  // Returns true if the object should self-delete.
  // 
  // 递减引用计数器。为 0 时，设置析构状态；并返回 true，表明该对象应立即删除。
  bool Release() const {
    // TODO(maruel): Add back once it doesn't assert 500 times/sec.
    // Current thread books the critical section "AddRelease"
    // without release it.
    // DFAKE_SCOPED_LOCK_THREAD_LOCKED(add_release_);
  #ifndef NDEBUG
    DCHECK(!in_dtor_);
  #endif
    if (--ref_count_ == 0) {
  #ifndef NDEBUG
      in_dtor_ = true;
  #endif
      return true;
    }
    return false;
  }

 private:
  // 引用计数器
  mutable int ref_count_;
  // 是否已析构(__clang__ 无析构)
#if defined(__clang__)
  mutable bool ALLOW_UNUSED  in_dtor_;
#else
  mutable bool in_dtor_;
#endif
  DFAKE_MUTEX(add_release_);

  DISALLOW_COPY_AND_ASSIGN(RefCountedBase);
};

// 线程安全引用计数器基类
class BUTIL_EXPORT RefCountedThreadSafeBase {
 public:
  // 是否只有一个引用计数器
  bool HasOneRef() const;

 protected:
  RefCountedThreadSafeBase();
  ~RefCountedThreadSafeBase();

  void AddRef() const;

  // Returns true if the object should self-delete.
  bool Release() const;

 private:
  mutable AtomicRefCount ref_count_;
#if defined(__clang__)
  mutable bool ALLOW_UNUSED  in_dtor_;
#else
  mutable bool in_dtor_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RefCountedThreadSafeBase);
};

}  // namespace subtle

//
// A base class for reference counted classes.  Otherwise, known as a cheap
// knock-off of WebKit's RefCounted<T> class.  To use this guy just extend your
// class from it like so:
//
//   class MyFoo : public butil::RefCounted<MyFoo> {
//    ...
//    private:
//     friend class butil::RefCounted<MyFoo>;
//     ~MyFoo();
//   };
//
// You should always make your destructor private, to avoid any code deleting
// the object accidently while there are references to it.
// 
// 引用计数器基类模版
// 
// Use like:
//   class MyFoo : public butil::RefCounted<MyFoo> {
//    ...
//    private:
//     friend class butil::RefCounted<MyFoo>;
//     ~MyFoo();
//   };
//
// 注：应该始终使你的析构函数为私有的，以避免任何代码在引用时意外删除该对象。
template <class T>
class RefCounted : public subtle::RefCountedBase {
 public:
  RefCounted() {}

  void AddRef() const {
    subtle::RefCountedBase::AddRef();
  }

  void Release() const {
    if (subtle::RefCountedBase::Release()) {
      delete static_cast<const T*>(this);
    }
  }

 protected:
  ~RefCounted() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RefCounted<T>);
};

// Forward declaration.
// 
// 引用计数器基类模版。前向声明
template <class T, typename Traits> class RefCountedThreadSafe;

// Default traits for RefCountedThreadSafe<T>.  Deletes the object when its ref
// count reaches 0.  Overload to delete it on a different thread etc.
// 
// RefCountedThreadSafe<T> 的默认 Traits 特征。用来当对象的引用计数达到 0 时删除该对象。
template<typename T>
struct DefaultRefCountedThreadSafeTraits {
  static void Destruct(const T* x) {
    // Delete through RefCountedThreadSafe to make child classes only need to be
    // friend with RefCountedThreadSafe instead of this struct, which is an
    // implementation detail.
    // 
    // 真正的销毁对象动作为 RefCountedThreadSafe<T, 
    //      DefaultRefCountedThreadSafeTraits>::DeleteInternal(x);
    //      该函数默认 delete 类型 T 对象。
    RefCountedThreadSafe<T,
                         DefaultRefCountedThreadSafeTraits>::DeleteInternal(x);
  }
};

//
// A thread-safe variant of RefCounted<T>
//
//   class MyFoo : public butil::RefCountedThreadSafe<MyFoo> {
//    ...
//   };
//
// If you're using the default trait, then you should add compile time
// asserts that no one else is deleting your object.  i.e.
//    private:
//     friend class butil::RefCountedThreadSafe<MyFoo>;
//     ~MyFoo();
//     
// 线程安全引用计数器模版。
// 
// Use like:
//   class MyFoo : public butil::RefCountedThreadSafe<MyFoo> {
//    ...
//   };
// 
// 注：应该始终使你的析构函数为私有的，以避免任何代码在引用时意外删除该对象。例如：
//    private:
//     friend class butil::RefCountedThreadSafe<MyFoo>; // 要求访问私有析构
//     ~MyFoo();
template <class T, typename Traits = DefaultRefCountedThreadSafeTraits<T> >
class RefCountedThreadSafe : public subtle::RefCountedThreadSafeBase {
 public:
  RefCountedThreadSafe() {}

  void AddRef() const {
    subtle::RefCountedThreadSafeBase::AddRef();
  }

  void Release() const {
    if (subtle::RefCountedThreadSafeBase::Release()) {
      Traits::Destruct(static_cast<const T*>(this));
    }
  }

 protected:
  ~RefCountedThreadSafe() {}

 private:
  friend struct DefaultRefCountedThreadSafeTraits<T>;
  static void DeleteInternal(const T* x) { delete x; }

  DISALLOW_COPY_AND_ASSIGN(RefCountedThreadSafe);
};

//
// A thread-safe wrapper for some piece of data so we can place other
// things in scoped_refptrs<>.
// 
// 一个线程安全的数据包装器。所以我们可以在 scoped_refptr<> 中放置其他的东西。
//
template<typename T>
class RefCountedData
    : public butil::RefCountedThreadSafe< butil::RefCountedData<T> > {
 public:
  RefCountedData() : data() {}
  RefCountedData(const T& in_value) : data(in_value) {}

  T data;

 private:
  friend class butil::RefCountedThreadSafe<butil::RefCountedData<T> >;
  ~RefCountedData() {}
};

}  // namespace butil

//
// A smart pointer class for reference counted objects.  Use this class instead
// of calling AddRef and Release manually on a reference counted object to
// avoid common memory leaks caused by forgetting to Release an object
// reference.  Sample usage:
//
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//   };
//
//   void some_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // |foo| is released when this function returns
//   }
//
//   void some_other_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     ...
//     foo = NULL;  // explicitly releases |foo|
//     ...
//     if (foo)
//       foo->Method(param);
//   }
//
// The above examples show how scoped_refptr<T> acts like a pointer to T.
// Given two scoped_refptr<T> classes, it is also possible to exchange
// references between the two objects, like so:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references NULL.
//   }
//
// To make both |a| and |b| in the above example reference the same MyFoo
// object, simply use the assignment operator:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//   }
//
// 引用计数对象的智能指针类。使用此类而不是在引用计数对象上手动调用 AddRef 和 Release ，
// 以避免由于忘记释放对象引用而导致的常见内存泄漏。
// 
// Use like:
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//   };
//
//   void some_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // 当 some_function 函数返回， |foo| 会被自动析构
//   }
//
//   void some_other_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     ...
//     foo = NULL;  // |foo| 被显示析构
//     ...
//     if (foo)
//       foo->Method(param);
//   }
//   
//   // scoped_refptr 交换 swap 操作：
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b.swap(a);
//     // 现在， |b| 引用了堆上的 MyFoo 对象， |a| 为空。
//   }
//   
//   // scoped_refptr 赋值操作：
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b = a;
//     // 现在， |b| 和 |b| 都引用了堆上的 MyFoo 对象。
//   }
template <class T>
class scoped_refptr {
 public:
  typedef T element_type;   // 对象类型

  scoped_refptr() : ptr_(NULL) {
  }

  // 递增引用计数器
  scoped_refptr(T* p) : ptr_(p) {
    if (ptr_)
      ptr_->AddRef();
  }

  // 递增引用计数器
  scoped_refptr(const scoped_refptr<T>& r) : ptr_(r.ptr_) {
    if (ptr_)
      ptr_->AddRef();
  }

  // 递增引用计数器
  // 要求 T 为 U 的基类，否则会出现编译错误（不同类型指针间赋值）。
  template <typename U>
  scoped_refptr(const scoped_refptr<U>& r) : ptr_(r.get()) {
    if (ptr_)
      ptr_->AddRef();
  }

  // 递减引用计数器
  ~scoped_refptr() {
    if (ptr_)
      ptr_->Release();
  }

  // 获取原始指针
  T* get() const { return ptr_; }

  // Allow scoped_refptr<C> to be used in boolean expression
  // and comparison operations.
  // 
  // 原始指针隐式转换（可以允许 scoped_refptr<C> 使用在布尔/关系表达式中）
  operator T*() const { return ptr_; }

  // 取指针操作符重载
  T* operator->() const {
    assert(ptr_ != NULL);
    return ptr_;
  }

  // 赋值操作符重载
  // 右边操作对象引用计数 +1 ；左边操作对象引用计数 -1
  scoped_refptr<T>& operator=(T* p) {
    // AddRef first so that self assignment should work
    // 
    // 自我赋值安全处理
    if (p)
      p->AddRef();
    T* old_ptr = ptr_;
    ptr_ = p;
    if (old_ptr)
      old_ptr->Release();
    return *this;
  }

  scoped_refptr<T>& operator=(const scoped_refptr<T>& r) {
    return *this = r.ptr_;
  }

  // 要求 T 为 U 的基类，否则会出现编译错误（不同类型指针间赋值）。
  template <typename U>
  scoped_refptr<T>& operator=(const scoped_refptr<U>& r) {
    return *this = r.get();
  }

  // 将智能指针管理的对象置换出去（释放所有权。注意：不再受智能指针管理）
  void swap(T** pp) {
    T* p = ptr_;
    ptr_ = *pp;
    *pp = p;
  }

  void swap(scoped_refptr<T>& r) {
    swap(&r.ptr_);
  }

  // Release ownership of ptr_, keeping its reference counter unchanged.
  // 
  // 释放 ptr_ 的所有权，保持其引用计数器不变（注意：不再受智能指针管理）
  T* release() WARN_UNUSED_RESULT {
      T* saved_ptr = NULL;
      swap(&saved_ptr);
      return saved_ptr;
  }

 protected:
  T* ptr_;  // 智能指针管理的底层原生的对象指针
};

// Handy utility for creating a scoped_refptr<T> out of a T* explicitly without
// having to retype all the template arguments
// 
// 用于创建一个 scoped_refptr<T> ，不需要输入所有的模板参数（利用函数模板自动推导特性）
template <typename T>
scoped_refptr<T> make_scoped_refptr(T* t) {
  return scoped_refptr<T>(t);
}

#endif  // BUTIL_MEMORY_REF_COUNTED_H_

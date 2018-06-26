// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The LazyInstance<Type, Traits> class manages a single instance of Type,
// which will be lazily created on the first time it's accessed.  This class is
// useful for places you would normally use a function-level static, but you
// need to have guaranteed thread-safety.  The Type constructor will only ever
// be called once, even if two threads are racing to create the object.  Get()
// and Pointer() will always return the same, completely initialized instance.
// When the instance is constructed it is registered with AtExitManager.  The
// destructor will be called on program exit.
//
// LazyInstance is completely thread safe, assuming that you create it safely.
// The class was designed to be POD initialized, so it shouldn't require a
// static constructor.  It really only makes sense to declare a LazyInstance as
// a global variable using the LAZY_INSTANCE_INITIALIZER initializer.
//
// LazyInstance is similar to Singleton, except it does not have the singleton
// property.  You can have multiple LazyInstance's of the same type, and each
// will manage a unique instance.  It also preallocates the space for Type, as
// to avoid allocating the Type instance on the heap.  This may help with the
// performance of creating the instance, and reducing heap fragmentation.  This
// requires that Type be a complete type so we can determine the size.
//
// Example usage:
//   static LazyInstance<MyClass> my_instance = LAZY_INSTANCE_INITIALIZER;
//   void SomeMethod() {
//     my_instance.Get().SomeMethod();  // MyClass::SomeMethod()
//
//     MyClass* ptr = my_instance.Pointer();
//     ptr->DoDoDo();  // MyClass::DoDoDo
//   }
//   
//  
// LazyInstance<Type, Traits> 类管理一个单一的 Type 实例，它将在第一次访问时被懒惰地
// 创建。这个类通常对于使用函数级别的静态 (function-level static) 的地方很有用，但是需
// 要保证线程安全。 Type 构造函数只会被调用一次，即使两个线程会 "竞争的" 来创建对象。Get() 
// 和 Pointer() 将始终返回相同的完全初始化的实例。当实例被构造时，它将被注册到 AtExitManager, 
// AtExitManager 对象析构函数将在程序进程退出时调用。
// 
// LazyInstance 是完全线程安全的，假设您安全地创建它。该类被设计为 POD 初始化，因此它不
// 应该需要静态的构造函数。只有将 LazyInstance 声明成全局变量，并使用 LAZY_INSTANCE_INITIALIZER 
// 初始化后才有意义。
// 
// LazyInstance 与 Singleton 类似，只是它没有 Singleton 属性。可以拥有多个相同类型的 LazyInstance，
// 并且每个都将管理一个唯一的实例。它还预先分配 Type 的空间，以避免在堆上分配 Type 实例。
// 这可能有助于创建实例的性能，并减少堆碎片。这要求 Type 是完整的类型，以便我们可以确定大小。
//  
//  Use like:
//   static LazyInstance<MyClass> my_instance = LAZY_INSTANCE_INITIALIZER;
//   void SomeMethod() {
//     my_instance.Get().SomeMethod();  // MyClass::SomeMethod()
//
//     MyClass* ptr = my_instance.Pointer();
//     ptr->DoDoDo();  // MyClass::DoDoDo
//   }

#ifndef BUTIL_LAZY_INSTANCE_H_
#define BUTIL_LAZY_INSTANCE_H_

#include <new>  // For placement new.

#include "butil/atomicops.h"
#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/debug/leak_annotations.h"
#include "butil/logging.h"
#include "butil/memory/aligned_memory.h"
#include "butil/third_party/dynamic_annotations/dynamic_annotations.h"
#include "butil/threading/thread_restrictions.h"

// LazyInstance uses its own struct initializer-list style static
// initialization, as base's LINKER_INITIALIZED requires a constructor and on
// some compilers (notably gcc 4.4) this still ends up needing runtime
// initialization.
// 
// LazyInstance 使用自己的 struct initializer-list 风格的静态初始化。在某些编译器
// （特别是 gcc 4.4）上，这仍然需要运行时初始化。
#define LAZY_INSTANCE_INITIALIZER { 0, {{0}} }

namespace butil {

// LazyInstance<Type> 的默认特征 traits 。在分配的内存资源上调用 placement new
template <typename Type>
struct DefaultLazyInstanceTraits {
  // 设置为 true 以自动注册进程退出时对象的删除器。
  static const bool kRegisterOnExit;
#ifndef NDEBUG
  // 设置为 false 以禁止在不可加入(non-joinable)的线程上访问。
  static const bool kAllowedToAccessOnNonjoinableThread;
#endif

  static Type* New(void* instance) {
    // 检测缓冲区传递给 placement new 是否对齐
    DCHECK_EQ(reinterpret_cast<uintptr_t>(instance) & (ALIGNOF(Type) - 1), 0u)
        << ": Bad boy, the buffer passed to placement new is not aligned!\n"
        "This may break some stuff like SSE-based optimizations assuming the "
        "<Type> objects are word aligned.";
    // Use placement new to initialize our instance in our preallocated space.
    // The parenthesis is very important here to force POD type initialization.
    // 
    // 使用 placement new 在我们的预分配空间中初始化我们的实例。括号在这里强制 POD 类型
    // 的初始化非常重要（调用构造函数）。
    return new (instance) Type();
  }
  static void Delete(Type* instance) {
    // Explicitly call the destructor.
    // 
    // 显式调用析构函数
    instance->~Type();
  }
};

// NOTE(gejun): BullseyeCoverage Compile C++ 8.4.4 complains about `undefined
// reference' on in-place assignments to static constants.
// 
// BullseyeCoverage C++ 8.4.4 编译器，对静态常量的就地赋值抱怨“未定义引用”。
template <typename Type>
const bool DefaultLazyInstanceTraits<Type>::kRegisterOnExit = true;
#ifndef NDEBUG
template <typename Type>
const bool DefaultLazyInstanceTraits<Type>::kAllowedToAccessOnNonjoinableThread = false;
#endif

// We pull out some of the functionality into non-templated functions, so we
// can implement the more complicated pieces out of line in the .cc file.
// 
// 将一些功能引入非模板化函数中，以便我们可以在 .cc 文件中实现更复杂的部分。
namespace internal {

// Use LazyInstance<T>::Leaky for a less-verbose call-site typedef; e.g.:
// butil::LazyInstance<T>::Leaky my_leaky_lazy_instance;
// instead of:
// butil::LazyInstance<T, butil::internal::LeakyLazyInstanceTraits<T> >
// my_leaky_lazy_instance;
// (especially when T is MyLongTypeNameImplClientHolderFactory).
// Only use this internal::-qualified verbose form to extend this traits class
// (depending on its implementation details).
// 
// 用于 LazyInstance<Type> 的备用特征 traits 。与 DefaultLazyInstanceTraits 相同，只是 
// LazyInstance 在退出时对象不会被析构。
// 
// Use like:
// butil::LazyInstance<T, butil::internal::LeakyLazyInstanceTraits<T> >
// my_leaky_lazy_instance;
// (特别是当 T 是 MyLongTypeNameImplClientHolderFactory 时)。
template <typename Type>
struct LeakyLazyInstanceTraits {
  static const bool kRegisterOnExit;
#ifndef NDEBUG
  static const bool kAllowedToAccessOnNonjoinableThread;
#endif

  // 构造 Type 对象
  static Type* New(void* instance) {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    return DefaultLazyInstanceTraits<Type>::New(instance);
  }
  // 注：对象不会被析构
  static void Delete(Type* instance) {
  }
};

template <typename Type>
const bool LeakyLazyInstanceTraits<Type>::kRegisterOnExit = false;
#ifndef NDEBUG
template <typename Type>
const bool LeakyLazyInstanceTraits<Type>::kAllowedToAccessOnNonjoinableThread = true;
#endif

// Our AtomicWord doubles as a spinlock, where a value of
// kBeingCreatedMarker means the spinlock is being held for creation.
// 
// AtomicWord 用作自旋锁，其中 kBeingCreatedMarker=1 表明对象正在被创建。
static const subtle::AtomicWord kLazyInstanceStateCreating = 1;

// Check if instance needs to be created. If so return true otherwise
// if another thread has beat us, wait for instance to be created and
// return false.
// 
// 检查本线程是否需要创建实例。如果是，则返回 true ，否则如果另一个线程正在创建
// 实例，则等待其创建完成并返回 false
BUTIL_EXPORT bool NeedsLazyInstance(subtle::AtomicWord* state);

// After creating an instance, call this to register the dtor to be called
// at program exit and to update the atomic state to hold the |new_instance|
// 
// 创建一个实例后，调用它来注册在程序退出时应该调用的 dtor ，并更新原子 |state| 以保
// 存 |new_instance|
BUTIL_EXPORT void CompleteLazyInstance(subtle::AtomicWord* state,
                                      subtle::AtomicWord new_instance,
                                      void* lazy_instance,
                                      void (*dtor)(void*));

}  // namespace internal

template <typename Type, typename Traits = DefaultLazyInstanceTraits<Type> >
class LazyInstance {
 public:
  // Do not define a destructor, as doing so makes LazyInstance a
  // non-POD-struct. We don't want that because then a static initializer will
  // be created to register the (empty) destructor with atexit() under MSVC, for
  // example. We handle destruction of the contained Type class explicitly via
  // the OnExit member function, where needed.
  // ~LazyInstance() {}
  // 
  // 不要定义析构函数，因为这样做会使 LazyInstance 成为非 POD 结构（不能有非默认类控制成员）。
  // 我们通过 OnExit 成员函数明确处理所包含 Type 类的销毁操作。

  // Convenience typedef to avoid having to repeat Type for leaky lazy
  // instances.
  // 
  // 可以方便引用懒惰泄漏 LazyInstance 类型 Type 。
  typedef LazyInstance<Type, internal::LeakyLazyInstanceTraits<Type> > Leaky;

  // 获取对象实例引用
  Type& Get() {
    return *Pointer();
  }

  // 获取对象指针
  Type* Pointer() {
#ifndef NDEBUG
    // Avoid making TLS lookup on release builds.
    // 
    // 避免在发布版本上进行 TLS 查找
    if (!Traits::kAllowedToAccessOnNonjoinableThread)
      ThreadRestrictions::AssertSingletonAllowed();
#endif
    // If any bit in the created mask is true, the instance has already been
    // fully constructed.
    // 
    // 如果创建的掩码中的任何位都为真，则实例已经完全构建。111...110
    static const subtle::AtomicWord kLazyInstanceCreatedMask =
        ~internal::kLazyInstanceStateCreating;

    // We will hopefully have fast access when the instance is already created.
    // Since a thread sees private_instance_ == 0 or kLazyInstanceStateCreating
    // at most once, the load is taken out of NeedsInstance() as a fast-path.
    // The load has acquire memory ordering as a thread which sees
    // private_instance_ > creating needs to acquire visibility over
    // the associated data (private_buf_). Pairing Release_Store is in
    // CompleteLazyInstance().
    // 
    // 当实例已经创建时，我们希望能够快速访问。由于线程最多只能看到 private_instance_ == 0 
    // 或 kLazyInstanceStateCreating 一次，因此负载将从 NeedsInstance() 中作为快速路径取出。
    // 一个线程获取原子值（考虑内存乱序执行），看到 private_instance_ > creating 需要获取相关
    // 数据（private_buf_）的可见性。配对 Release_Store 在 CompleteLazyInstance() 中。
    // 
    // 原子读取 private_instance_ 保存的实例指针值。
    // 当 value 是有效的 ptr ， value & kLazyInstanceCreatedMask 不为 0 。
    subtle::AtomicWord value = subtle::Acquire_Load(&private_instance_);
    if (!(value & kLazyInstanceCreatedMask) &&
        internal::NeedsLazyInstance(&private_instance_)) {
      // Create the instance in the space provided by |private_buf_|.
      // 
      // 在由 |private_buf_| 提供的内存空间中创建实例。
      value = reinterpret_cast<subtle::AtomicWord>(
          Traits::New(private_buf_.void_data()));
      // 注册实例的销毁回调函数。并将 private_buf_.void_data() 值写入 private_instance_ 中。
      internal::CompleteLazyInstance(&private_instance_, value, this,
                                     Traits::kRegisterOnExit ? OnExit : NULL);
    }

    // This annotation helps race detectors recognize correct lock-less
    // synchronization between different threads calling Pointer().
    // We suggest dynamic race detection tool that "Traits::New" above
    // and CompleteLazyInstance(...) happens before "return instance()" below.
    // See the corresponding HAPPENS_BEFORE in CompleteLazyInstance(...).
    ANNOTATE_HAPPENS_AFTER(&private_instance_);
    // 创建实例指针
    return instance();
  }

  bool operator==(Type* p) {
    switch (subtle::NoBarrier_Load(&private_instance_)) {
      case 0:
        return p == NULL;
      case internal::kLazyInstanceStateCreating:
      // 本单例实例正在创建（比较内存地址）
        return static_cast<void*>(p) == private_buf_.void_data();
      default:
        return p == instance();
    }
  }

  // Effectively private: member data is only public to allow the linker to
  // statically initialize it and to maintain a POD class. DO NOT USE FROM
  // OUTSIDE THIS CLASS.

  // 存储单例实例的指针（实例创建完成后，其值等于 private_buf_.void_data() ）
  subtle::AtomicWord private_instance_;
  // Preallocated space for the Type instance.
  // 
  // 为 Type 实例预分配的内存空间（编译时确定大小）
  butil::AlignedMemory<sizeof(Type), ALIGNOF(Type)> private_buf_;

 private:
  Type* instance() {
    return reinterpret_cast<Type*>(subtle::NoBarrier_Load(&private_instance_));
  }

  // Adapter function for use with AtExit.  This should be called single
  // threaded, so don't synchronize across threads.
  // Calling OnExit while the instance is in use by other threads is a mistake.
  // 
  // 与 AtExit 一起使用的适配器函数。这应该被单线程调用，所以不要跨线程。当实例被其他线程使用
  // 时调用 OnExit 是一个错误！
  static void OnExit(void* lazy_instance) {
    LazyInstance<Type, Traits>* me =
        reinterpret_cast<LazyInstance<Type, Traits>*>(lazy_instance);
    // 析构对象
    Traits::Delete(me->instance());
    // 重置指针值到未构造状态
    subtle::NoBarrier_Store(&me->private_instance_, 0);
  }
};

}  // namespace butil

#endif  // BUTIL_LAZY_INSTANCE_H_

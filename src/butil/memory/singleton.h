// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE READ: Do you really need a singleton?
//
// Singletons make it hard to determine the lifetime of an object, which can
// lead to buggy code and spurious crashes.
//
// Instead of adding another singleton into the mix, try to identify either:
//   a) An existing singleton that can manage your object's lifetime
//   b) Locations where you can deterministically create the object and pass
//      into other objects
//
// If you absolutely need a singleton, please keep them as trivial as possible
// and ideally a leaf dependency. Singletons get problematic when they attempt
// to do too much in their destructor or have circular dependencies.
// 
// 必要时才使用单例。单例使得很难确定一个对象的生命周期，导致错误的代码或错误的崩溃信息。
// 
// 尽量不使用单例，请尝试识别：
// a）一个可以管理你的对象的生命周期的现有单例
// b）确定性地创建对象并传入其他对象的位置
// 
// 如果你确定需要一个单例，请尽可能保持简单，最好是没有依赖。当单例试图在析构函数中做太多或者
// 具有循环依赖时，单例会遇到问题。

#ifndef BUTIL_MEMORY_SINGLETON_H_
#define BUTIL_MEMORY_SINGLETON_H_

#include "butil/at_exit.h"
#include "butil/atomicops.h"
#include "butil/base_export.h"
#include "butil/memory/aligned_memory.h"
#include "butil/third_party/dynamic_annotations/dynamic_annotations.h"
#include "butil/threading/thread_restrictions.h"

namespace butil {
namespace internal {

// Our AtomicWord doubles as a spinlock, where a value of
// kBeingCreatedMarker means the spinlock is being held for creation.
// 
// AtomicWord 用作自旋锁，其中 kBeingCreatedMarker=1 表明对象正在被创建。
static const subtle::AtomicWord kBeingCreatedMarker = 1;

// We pull out some of the functionality into a non-templated function, so that
// we can implement the more complicated pieces out of line in the .cc file.
// 
// 将一些功能引入非模板化函数中，以便我们可以在 .cc 文件中实现更复杂的部分。
// 线程间自旋等待实例创建完成。
BUTIL_EXPORT subtle::AtomicWord WaitForInstance(subtle::AtomicWord* instance);

}  // namespace internal
}  // namespace butil

// TODO(joth): Move more of this file into namespace butil

// Default traits for Singleton<Type>. Calls operator new and operator delete on
// the object. Registers automatic deletion at process exit.
// Overload if you need arguments or another memory allocation function.
// 
// Singleton<Type> 的默认特征 traits 。在对象上调用 operator new 和 operator delete 
// 在进程退出时自动删除。如果需要参数或其他内存分配函数，则重载之。
template<typename Type>
struct DefaultSingletonTraits {
  // Allocates the object.
  // 
  // 分配对象
  static Type* New() {
    // The parenthesis is very important here; it forces POD type
    // initialization.
    // 
    // 括号在这里非常重要：它强制 POD 类型初始化（调用构造函数）。
    return new Type();
  }

  // Destroys the object.
  // 
  // 释放对象
  static void Delete(Type* x) {
    delete x;
  }

  // Set to true to automatically register deletion of the object on process
  // exit. See below for the required call that makes this happen.
  // 
  // 设置为 true 以自动注册进程退出时对象的删除器。
  static const bool kRegisterAtExit;

#ifndef NDEBUG
  // Set to false to disallow access on a non-joinable thread.  This is
  // different from kRegisterAtExit because StaticMemorySingletonTraits allows
  // access on non-joinable threads, and gracefully handles this.
  // 
  // 设置为 false 以禁止在不可加入(non-joinable)的线程上访问。这与 kRegisterAtExit 不
  // 同，因为 StaticMemorySingletonTraits 允许访问不可加入(non-joinable)的线程，并优
  // 雅地处理。
  static const bool kAllowedToAccessOnNonjoinableThread;
#endif
};

// NOTE(gejun): BullseyeCoverage Compile C++ 8.4.4 complains about `undefined
// reference' on in-place assignments to static constants.
// 
// BullseyeCoverage C++ 8.4.4 编译器，对静态常量的就地赋值抱怨“未定义引用”。
template<typename Type>
const bool DefaultSingletonTraits<Type>::kRegisterAtExit = true;
#ifndef NDEBUG
template<typename Type>
const bool DefaultSingletonTraits<Type>::kAllowedToAccessOnNonjoinableThread = false;
#endif

// Alternate traits for use with the Singleton<Type>.  Identical to
// DefaultSingletonTraits except that the Singleton will not be cleaned up
// at exit.
// 
// 用于 Singleton<Type> 的备用特征 traits 。与 DefaultSingletonTraits 相同，只是 
// Singleton 在退出时不会被清理。
template<typename Type>
struct LeakySingletonTraits : public DefaultSingletonTraits<Type> {
  static const bool kRegisterAtExit;
#ifndef NDEBUG
  static const bool kAllowedToAccessOnNonjoinableThread;
#endif
};

template<typename Type>
const bool LeakySingletonTraits<Type>::kRegisterAtExit = false;
#ifndef NDEBUG
template<typename Type>
const bool LeakySingletonTraits<Type>::kAllowedToAccessOnNonjoinableThread = true;
#endif

// Alternate traits for use with the Singleton<Type>.  Allocates memory
// for the singleton instance from a static buffer.  The singleton will
// be cleaned up at exit, but can't be revived after destruction unless
// the Resurrect() method is called.
//
// This is useful for a certain category of things, notably logging and
// tracing, where the singleton instance is of a type carefully constructed to
// be safe to access post-destruction.
// In logging and tracing you'll typically get stray calls at odd times, like
// during static destruction, thread teardown and the like, and there's a
// termination race on the heap-based singleton - e.g. if one thread calls
// get(), but then another thread initiates AtExit processing, the first thread
// may call into an object residing in unallocated memory. If the instance is
// allocated from the data segment, then this is survivable.
//
// The destructor is to deallocate system resources, in this case to unregister
// a callback the system will invoke when logging levels change. Note that
// this is also used in e.g. Chrome Frame, where you have to allow for the
// possibility of loading briefly into someone else's process space, and
// so leaking is not an option, as that would sabotage the state of your host
// process once you've unloaded.
// 
// 用于 Singleton<Type> 的备用特征 traits 。为来自静态缓冲区上构造单例实例。单例实例在
// 退出时会被清理掉，但是除非调用 Resurrect() 方法，否则不会在被销毁后恢复。
// 
// 这对于某些类别的事物非常有用，特别是日志记录和跟踪，其中单例实例是一种精心构造的类型，可
// 以安全地访问销毁后的内容。在日志记录和追踪中，你通常会在奇怪的时间得到迷失的调用，比如在
// 静态销毁，线程拆解等等情况下，并且在基于堆的单例中有终止比赛。如果一个线程调用 get() ，
// 但另一个线程启动 AtExit 处理，则第一个线程可能会调用驻留在未分配内存中的对象。如果实例是
// 从数据段分配的，那么这是可以生存的。
// 
// 析构函数将解除分配系统资源，在这种情况下，注销系统将在日志级别更改时调用的回调。注意，这也
// 用于例如 Chrome Frame，在这里你必须允许短暂加载到其他人的进程空间，因此泄漏不是一种选择，
// 因为这会在你卸载后破坏主机进程的状态。
template <typename Type>
struct StaticMemorySingletonTraits {
  // WARNING: User has to deal with get() in the singleton class
  // this is traits for returning NULL.
  // 
  // 用户必须在单例类中处理 get() ，这是返回 NULL 的特征
  static Type* New() {
    // Only constructs once and returns pointer; otherwise returns NULL.
    // 
    // 只构造一次并返回指针; 否则返回 NULL
    if (butil::subtle::NoBarrier_AtomicExchange(&dead_, 1))
      return NULL;

    // placement new
    return new(buffer_.void_data()) Type();
  }

  static void Delete(Type* p) {
    if (p != NULL)
      p->Type::~Type();
  }

  static const bool kRegisterAtExit = true;
  static const bool kAllowedToAccessOnNonjoinableThread = true;

  // Exposed for unittesting.
  // 
  // 暴露给单元测试
  static void Resurrect() {
    butil::subtle::NoBarrier_Store(&dead_, 0);
  }

 private:
  // 对象的静态内存缓冲区
  static butil::AlignedMemory<sizeof(Type), ALIGNOF(Type)> buffer_;
  // Signal the object was already deleted, so it is not revived.
  // 
  // 标记对象已被删除，所以它不会恢复
  static butil::subtle::Atomic32 dead_;
};

template <typename Type> butil::AlignedMemory<sizeof(Type), ALIGNOF(Type)>
    StaticMemorySingletonTraits<Type>::buffer_;
template <typename Type> butil::subtle::Atomic32
    StaticMemorySingletonTraits<Type>::dead_ = 0;

// The Singleton<Type, Traits, DifferentiatingType> class manages a single
// instance of Type which will be created on first use and will be destroyed at
// normal process exit). The Trait::Delete function will not be called on
// abnormal process exit.
//
// DifferentiatingType is used as a key to differentiate two different
// singletons having the same memory allocation functions but serving a
// different purpose. This is mainly used for Locks serving different purposes.
//
// Example usage:
//
// In your header:
//   template <typename T> struct DefaultSingletonTraits;
//   class FooClass {
//    public:
//     static FooClass* GetInstance();  <-- See comment below on this.
//     void Bar() { ... }
//    private:
//     FooClass() { ... }
//     friend struct DefaultSingletonTraits<FooClass>;
//
//     DISALLOW_COPY_AND_ASSIGN(FooClass);
//   };
//
// In your source file:
//  #include "butil/memory/singleton.h"
//  FooClass* FooClass::GetInstance() {
//    return Singleton<FooClass>::get();
//  }
//
// And to call methods on FooClass:
//   FooClass::GetInstance()->Bar();
//
// NOTE: The method accessing Singleton<T>::get() has to be named as GetInstance
// and it is important that FooClass::GetInstance() is not inlined in the
// header. This makes sure that when source files from multiple targets include
// this header they don't end up with different copies of the inlined code
// creating multiple copies of the singleton.
//
// Singleton<> has no non-static members and doesn't need to actually be
// instantiated.
//
// This class is itself thread-safe. The underlying Type must of course be
// thread-safe if you want to use it concurrently. Two parameters may be tuned
// depending on the user's requirements.
//
// Glossary:
//   RAE = kRegisterAtExit
//
// On every platform, if Traits::RAE is true, the singleton will be destroyed at
// process exit. More precisely it uses butil::AtExitManager which requires an
// object of this type to be instantiated. AtExitManager mimics the semantics
// of atexit() such as LIFO order but under Windows is safer to call. For more
// information see at_exit.h.
//
// If Traits::RAE is false, the singleton will not be freed at process exit,
// thus the singleton will be leaked if it is ever accessed. Traits::RAE
// shouldn't be false unless absolutely necessary. Remember that the heap where
// the object is allocated may be destroyed by the CRT anyway.
//
// Caveats:
// (a) Every call to get(), operator->() and operator*() incurs some overhead
//     (16ns on my P4/2.8GHz) to check whether the object has already been
//     initialized.  You may wish to cache the result of get(); it will not
//     change.
//
// (b) Your factory function must never throw an exception. This class is not
//     exception-safe.
//
//
// Singleton<Type, Traits, DifferentiatingType> 类管理一个 Type 的实例，它将在首次使
// 用时创建，并在正常的进程退出时被销毁。 Trait::Delete 函数在异常处理退出时不会被调用。
// 
// DifferentiatingType 用作区分具有相同内存分配功能但用于不同目的的两个不同单例的关键。
// 这主要用于服务于不同目的的锁。
// 
// Use like:
// \file *.h:
//   template <typename T> struct DefaultSingletonTraits;
//   class FooClass {
//    public:
//     static FooClass* GetInstance();  // See comment below on this.
//     void Bar() { ... }
//    private:
//     FooClass() { ... }
//     friend struct DefaultSingletonTraits<FooClass>;
//
//     DISALLOW_COPY_AND_ASSIGN(FooClass);
//   };
//
// \file *.cc:
//  #include "butil/memory/singleton.h"
//  FooClass* FooClass::GetInstance() {
//    return Singleton<FooClass>::get();
//  }
//
// And to call methods on FooClass:
//   FooClass::GetInstance()->Bar();
//   
// 注意：访问 Singleton<T>::get() 的方法必须命名为 GetInstance （该函数被声明为 Singleton<T> 
// 友元，用来访问私有 Singleton<T>::get()），重要的是 FooClass::GetInstance() 不在类内部。这
// 确保了当多个目标的源文件包含此头时，它们不会创建多个单例副本的内联代码的不同副本。
// 
// Singleton<> 没有非静态成员，并且不需要实际实例化。这个类本身是线程安全的。如果你想同时
// 使用它，底层 Type 必须是线程安全的。根据用户的要求，可以调整两个参数：
// Glossary:
//   RAE = kRegisterAtExit
//   
// 在每个平台上，如果 Traits::RAE 为 true ， 那么在进程退出时单例将被销毁。更准确地说，它
// 使用了 butil::AtExitManager ，它需要实例化这种类型的对象。 AtExitManager 模仿 atexit()
// 的语义，例如 LIFO 顺序，但在 Windows 下更安全。 有关更多信息，请参阅 at_exit.h 。
// 
// 如果 Traits::RAE 为 false ，则在进程退出时单例不会被释放，因此单例在被访问时会被泄漏。
// Traits::RAE 不应该是错误的，除非绝对必要。请记住，无论如何，CRT 所销毁的对象所在的堆可
// 能会被销毁。
// 
// 注意事项：
// (a) 每次调用 get(), operator->() 和 operator*() 会产生一些开销（我的 P4/2.8GHz 上的 16ns ）
//    来检查对象是否已经被初始化。你可能希望缓存 get() 的结果; 它不会改变。
// (b) 你的工厂函数绝对不能抛出异常。这个类不是异常安全的。
template <typename Type,
          typename Traits = DefaultSingletonTraits<Type>,
          typename DifferentiatingType = Type>
class Singleton {
 private:
  // Classes using the Singleton<T> pattern should declare a GetInstance()
  // method and call Singleton::get() from within that.
  // 
  // 使用 Singleton<T> 模式的类内部应该声明一个 GetInstance() 方法，并从该类的内部调用 
  // Singleton::get() 。这也起到规定访问 Singleton<T>::get() 的方法必须命名为 GetInstance
  friend Type* Type::GetInstance();

  // Allow TraceLog tests to test tracing after OnExit.
  // 
  // 允许 TraceLog 测试在 OnExit 之后测试跟踪
  friend class DeleteTraceLogForTesting;

  // This class is safe to be constructed and copy-constructed since it has no
  // member.
  // 
  // 由于该类没有成员，因此该类可以安全地构建和复制

  // Return a pointer to the one true instance of the class.
  // 
  // 返回指向类的一个单例实例的指针
  static Type* get() {
    // The load has acquire memory ordering as the thread which reads the
    // instance_ pointer must acquire visibility over the singleton data.
    // 
    // 原子读取 instance_ 保存的实例指针值。
    // 当 value != NULL ，它可能是 kBeingCreatedMarker 或有效的 ptr 。
    butil::subtle::AtomicWord value = butil::subtle::Acquire_Load(&instance_);
    if (value != 0 && value != butil::internal::kBeingCreatedMarker) {
      // See the corresponding HAPPENS_BEFORE below.
      ANNOTATE_HAPPENS_AFTER(&instance_);
      return reinterpret_cast<Type*>(value);
    }

    // Object isn't created yet, maybe we will get to create it, let's try...
    // 
    // 对象尚未创建，尝试创建它
    if (butil::subtle::Acquire_CompareAndSwap(
          &instance_, 0, butil::internal::kBeingCreatedMarker) == 0) {
      // instance_ was NULL and is now kBeingCreatedMarker.  Only one thread
      // will ever get here.  Threads might be spinning on us, and they will
      // stop right after we do this store.
      // 
      // instance_ 是 NULL ，现在是 kBeingCreatedMarker 。只有一个线程会到达这里。
      // 线程可能旋转在此处，他们会在我们完成这个存储后停下来。
      Type* newval = Traits::New();

      // This annotation helps race detectors recognize correct lock-less
      // synchronization between different threads calling get().
      // See the corresponding HAPPENS_AFTER below and above.
      // 
      // 这个注解有助于条件竞争检测器识别调用 get() 的不同线程之间正确的无锁同步。
      ANNOTATE_HAPPENS_BEFORE(&instance_);
      // Releases the visibility over instance_ to the readers.
      // 
      // 通过对 instance_ 的 Release_Store() 操作，向其他读线程发布写操作可见性（配
      // 合 WaitForInstance/get 中 Acquire_Load() 完成内存读写顺序执行一致性 ）。
      butil::subtle::Release_Store(
          &instance_, reinterpret_cast<butil::subtle::AtomicWord>(newval));

      // 注册单例实例的退出清理函数
      if (newval != NULL && Traits::kRegisterAtExit)
        butil::AtExitManager::RegisterCallback(OnExit, NULL);

      return newval;
    }

    // We hit a race. Wait for the other thread to complete it.
    // 
    // 自旋等待创建线程完成创建
    value = butil::internal::WaitForInstance(&instance_);

    // See the corresponding HAPPENS_BEFORE above.
    ANNOTATE_HAPPENS_AFTER(&instance_);
    return reinterpret_cast<Type*>(value);
  }

  // Adapter function for use with AtExit().  This should be called single
  // threaded, so don't use atomic operations.
  // Calling OnExit while singleton is in use by other threads is a mistake.
  // 
  // 适配用于 AtExit() 的功能。这应该被称为单线程，所以不要使用原子操作。当单例正在被
  // 其他线程使用时调用 OnExit 是一个错误。
  static void OnExit(void* /*unused*/) {
    // AtExit should only ever be register after the singleton instance was
    // created.  We should only ever get here with a valid instance_ pointer.
    // 
    // AtExit 应该只能在创建单例实例后注册。这里我们只能使用有效的 instance_ 指针
    Traits::Delete(
        reinterpret_cast<Type*>(butil::subtle::NoBarrier_Load(&instance_)));
    instance_ = 0;
  }

  // 存储单例实例的指针
  static butil::subtle::AtomicWord instance_;
};

// 初始化单例值
template <typename Type, typename Traits, typename DifferentiatingType>
butil::subtle::AtomicWord Singleton<Type, Traits, DifferentiatingType>::
    instance_ = 0;

#endif  // BUTIL_MEMORY_SINGLETON_H_

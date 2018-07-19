// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_THREADING_THREAD_LOCAL_STORAGE_H_
#define BUTIL_THREADING_THREAD_LOCAL_STORAGE_H_

// @tips
// 一、线程局部存储(TLS)
// 线程局部存储提供了持久的每个线程存储空间，每个线程都拥有一份对变量的拷贝。线程局部存
// 储中的变量将一直存在，直到线程终止，届时会自动释放这一存储。要定义一个线程局部变量很
// 简单，只需在|全局或静态|变量声明中简单的包含 __thread 说明符即可。
// 
// Use like:
// static __thread int buf[MAX_ERROR_LEN];
// 
// 注意：在一个线程中可以修改另一个线程的局部变量：__thread 变量并不是在线程之间完全隐
// 藏的，每个线程都保存自己的一份拷贝，因此每个线程的这个变量的地址不同。但这个地址是整
// 个进程可见的。因此一个线程获得另外一个线程的局部变量的地址，就可以修改另一个线程的这
// 个局部变量。
// 
// 线程局部变量的声明和使用，需要注意以下几点：
// 1. 如果变量声明中使用了关键字 static 或 extern ，那么关键字 __thread 必须紧随
//  其后。
// 2. 与一般的全局或静态变量声明一样，线程局部变量在声明时可以设置一个初始值。
// 3. 可以使用 C 语言取址操作符（&）来获取线程局部变量的地址。
// 
// C++ 中对 __thread 变量的使用有额外的限制：
// 1. 在 C++ 中，如果要在定义一个 thread-local 变量的时候做初始化，初始化的值必须是
//  一个常量表达式。
// 2. __thread 只能修饰 POD 类型（只能用其指针类型）。
// 
// 
// 从 C++11 后添加了 thread_local 关键字。
// 
// 
// 二、线程特有数据 <pthread.h>
// __thread 是 C/C++ 编译器层实现的使用线程级局部变量的方式。而 POSIX thread 则使
// 用 getthreadspecific() 和 setthreadspecific() 组件来实现这一特性，因此编译要
// 加 -pthread 。注：使用这种方式很繁琐，并且效率低。
// 
// 使用线程特有数据需要下面几步：
// 1. 创建一个键 (key) ，用以将不同的线程特有数据区分开来。 pthread_key_create() 
//  可创建一个 key，且只需要在首个调用该函数的线程中创建一次。
// 2. 在不同线程中，使用 pthread_setspecific() 函数将这个 key 和本线程（调用者线程）
//  中的某个变量的值关联起来，这样就可以做到不同线程使用相同的 key 保存不同的 value 。
// 3. 在各线程可通过 pthread_getspecific() 函数来取得本线程中 key 对应的值。
// 
// Linux 支持最多 1024 个 key ，一般是 128 个，所以通常 key 是够用的。如果一个函数
// 需要多个线程特有数据的值，可以将它们封装为一个结构体，然后仅与一个 key 关联。
// 
// 
// int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
// 用于创建一个 key，成功返回 0 。函数 destructor 指向一个自定义函数，在线程终止时，
// 会自动执行该函数进行一些析构动作。例如，释放与 key 绑定的存储空间的资源，如果无需解构，
// 可将 destructor 置为 NULL（destructor 函数参数 |void* value| 是与 key 关联的
// 指向线程特有数据块的指针。注意，如果一个线程有多个"线程特有数据块"，那么对各个解构函数
// 的调用顺序是不确定的，因此每个解构函数的设计要相互独立）。
// 
// int pthread_setspecific(pthread_key_t key, const void * value);
// 用于设置 key 与本线程内某个指针或某个值的关联。成功返回 0 。
// 
// void *pthread_getspecific(pthread_key_t key);
// 用于获取 key 关联的值，由该函数的返回值的指针指向。如果 key 在该线程中尚未被关联，该
// 函数返回 NULL 。
// 
// int pthread_key_delete(pthread_key_t key);
// 用于注销一个 key ，以供下一次调用 pthread_key_create() 使用。

#include "butil/base_export.h"
#include "butil/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <pthread.h>
#endif

namespace butil {

namespace internal {

// WARNING: You should *NOT* be using this class directly.
// PlatformThreadLocalStorage is low-level abstraction to the OS's TLS
// interface, you should instead be using ThreadLocalStorage::StaticSlot/Slot.
// 
// 警告：不要直接使用这个类。
// PlatformThreadLocalStorage 是对操作系统的 TLS 接口的低级抽象接口。请使用 
// ThreadLocalStorage::StaticSlot/Slot
class BUTIL_EXPORT PlatformThreadLocalStorage {
 public:

#if defined(OS_WIN)
  typedef unsigned long TLSKey;
  enum { TLS_KEY_OUT_OF_INDEXES = TLS_OUT_OF_INDEXES };
#elif defined(OS_POSIX)
  typedef pthread_key_t TLSKey;
  // The following is a "reserved key" which is used in our generic Chromium
  // ThreadLocalStorage implementation.  We expect that an OS will not return
  // such a key, but if it is returned (i.e., the OS tries to allocate it) we
  // will just request another key.
  // 
  // 以下是在通用 Chromium ThreadLocalStorage 实现中为“保留键”。我们期望操作系统不会
  // 返回这样的 key ，但是如果它返回（即操作系统试图分配它），我们需请求另一个 key 。
  enum { TLS_KEY_OUT_OF_INDEXES = 0x7FFFFFFF };
#endif

  // The following methods need to be supported on each OS platform, so that
  // the Chromium ThreadLocalStore functionality can be constructed.
  // Chromium will use these methods to acquire a single OS slot, and then use
  // that to support a much larger number of Chromium slots (independent of the
  // OS restrictions).
  // The following returns true if it successfully is able to return an OS
  // key in |key|.
  // 
  // 创建一个 |key| ，成功则返回 true 。 posix 平台调用 pthread_key_create()
  static bool AllocTLS(TLSKey* key);
  // Note: FreeTLS() doesn't have to be called, it is fine with this leak, OS
  // might not reuse released slot, you might just reset the TLS value with
  // SetTLSValue().
  // 
  // 销毁一个 |key|
  static void FreeTLS(TLSKey key);
  static void SetTLSValue(TLSKey key, void* value);
  static void* GetTLSValue(TLSKey key);

  // Each platform (OS implementation) is required to call this method on each
  // terminating thread when the thread is about to terminate.  This method
  // will then call all registered destructors for slots in Chromium
  // ThreadLocalStorage, until there are no slot values remaining as having
  // been set on this thread.
  // Destructors may end up being called multiple times on a terminating
  // thread, as other destructors may re-set slots that were previously
  // destroyed.
#if defined(OS_WIN)
  // Since Windows which doesn't support TLS destructor, the implementation
  // should use GetTLSValue() to retrieve the value of TLS slot.
  static void OnThreadExit();
#elif defined(OS_POSIX)
  // |Value| is the data stored in TLS slot, The implementation can't use
  // GetTLSValue() to retrieve the value of slot as it has already been reset
  // in Posix.
  static void OnThreadExit(void* value);
#endif
};

}  // namespace internal

// Wrapper for thread local storage.  This class doesn't do much except provide
// an API for portability.
// 
// 线程局部存储 TLS 包装器
class BUTIL_EXPORT ThreadLocalStorage {
 public:

  // Prototype for the TLS destructor function, which can be optionally used to
  // cleanup thread local storage on thread exit.  'value' is the data that is
  // stored in thread local storage.
  // 
  // TLS 析构原型， |value| 是线程本地存储区域
  typedef void (*TLSDestructorFunc)(void* value);

  // StaticSlot uses its own struct initializer-list style static
  // initialization, as base's LINKER_INITIALIZED requires a constructor and on
  // some compilers (notably gcc 4.4) this still ends up needing runtime
  // initialization.
#define TLS_INITIALIZER {false, 0}

  // A key representing one value stored in TLS.
  // Initialize like
  //   ThreadLocalStorage::StaticSlot my_slot = TLS_INITIALIZER;
  // If you're not using a static variable, use the convenience class
  // ThreadLocalStorage::Slot (below) instead.
  // 
  // struct StaticSlot 静态初始化
  struct BUTIL_EXPORT StaticSlot {
    // Set up the TLS slot.  Called by the constructor.
    // 'destructor' is a pointer to a function to perform per-thread cleanup of
    // this object.  If set to NULL, no cleanup is done for this TLS slot.
    // Returns false on error.
    bool Initialize(TLSDestructorFunc destructor);

    // Free a previously allocated TLS 'slot'.
    // If a destructor was set for this slot, removes
    // the destructor so that remaining threads exiting
    // will not free data.
    void Free();

    // Get the thread-local value stored in slot 'slot'.
    // Values are guaranteed to initially be zero.
    void* Get() const;

    // Set the thread-local value stored in slot 'slot' to
    // value 'value'.
    void Set(void* value);

    bool initialized() const { return initialized_; }

    // The internals of this struct should be considered private.
    bool initialized_;
    int slot_;
  };

  // A convenience wrapper around StaticSlot with a constructor. Can be used
  // as a member variable.
  class BUTIL_EXPORT Slot : public StaticSlot {
   public:
    // Calls StaticSlot::Initialize().
    explicit Slot(TLSDestructorFunc destructor = NULL);

   private:
    using StaticSlot::initialized_;
    using StaticSlot::slot_;

    DISALLOW_COPY_AND_ASSIGN(Slot);
  };

  DISALLOW_COPY_AND_ASSIGN(ThreadLocalStorage);
};

}  // namespace butil

#endif  // BUTIL_THREADING_THREAD_LOCAL_STORAGE_H_

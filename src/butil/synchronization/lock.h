// Copyright (c) 2017 Baidu, Inc
// Date: Thu Jan 19 16:19:30 CST 2017

// 跨平台互斥体包装器

#ifndef BUTIL_SYNCHRONIZATION_LOCK_H_
#define BUTIL_SYNCHRONIZATION_LOCK_H_

#include "butil/build_config.h"
#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <pthread.h>
#endif

#include "butil/base_export.h"
#include "butil/macros.h"
#include "butil/compat.h"

namespace butil {
// @tips
// critical section 称为代码关键段或者临界区域，它并不是核心对象，
// 不是属于操作系统维护的而是属于进程维护的，用它可以解决多线程同步技术

// A convenient wrapper for an OS specific critical section.  
// 
// 跨平台互斥体包装器
class BUTIL_EXPORT Mutex {
    DISALLOW_COPY_AND_ASSIGN(Mutex);
public:
// 跨平台互斥体类型
#if defined(OS_WIN)
  typedef CRITICAL_SECTION NativeHandle;
#elif defined(OS_POSIX)
  typedef pthread_mutex_t NativeHandle;
#endif

public:
    Mutex() {
#if defined(OS_WIN)
    // The second parameter is the spin count, for short-held locks it avoid the
    // contending thread from going to sleep which helps performance greatly.
    // 
    // 第二个参数为自旋锁计数器。对于短锁，它避免了竞争线程进入睡眠，这有助于高并发
        ::InitializeCriticalSectionAndSpinCount(&_native_handle, 2000);
#elif defined(OS_POSIX)
        pthread_mutex_init(&_native_handle, NULL);
#endif
    }
    
    ~Mutex() {
    // 释放互斥体
#if defined(OS_WIN)
        ::DeleteCriticalSection(&_native_handle);
#elif defined(OS_POSIX)
        pthread_mutex_destroy(&_native_handle);
#endif
    }

    // Locks the mutex. If another thread has already locked the mutex, a call
    // to lock will block execution until the lock is acquired.
    // 
    // 锁定互斥体。如果另一个线程已经锁定互斥体，则阻塞锁定的调用，直到可以获取锁
    void lock() {
#if defined(OS_WIN)
        ::EnterCriticalSection(&_native_handle);
#elif defined(OS_POSIX)
        pthread_mutex_lock(&_native_handle);
#endif
    }

    // Unlocks the mutex. The mutex must be locked by the current thread of
    // execution, otherwise, the behavior is undefined.
    // 
    // 释放互斥体。将被释放的锁定互斥体必须是当前线程锁定的，否则行为未定义！
    void unlock() {
#if defined(OS_WIN)
        ::LeaveCriticalSection(&_native_handle);
#elif defined(OS_POSIX)
        pthread_mutex_unlock(&_native_handle);
#endif
    }
    
    // Tries to lock the mutex. Returns immediately.
    // On successful lock acquisition returns true, otherwise returns false.
    // 
    // 尝试获取锁。 lock() 的非阻塞版本
    // 获取成功返回 true, 失败返回 false
    bool try_lock() {
#if defined(OS_WIN)
        return (::TryEnterCriticalSection(&_native_handle) != FALSE);
#elif defined(OS_POSIX)
        return pthread_mutex_trylock(&_native_handle) == 0;
#endif
    }

    // Returns the underlying implementation-defined native handle object.
    // 
    // 返回底层实现定义的原生互斥体句柄指针
    NativeHandle* native_handle() { return &_native_handle; }

private:
#if defined(OS_POSIX)
    // The posix implementation of ConditionVariable needs to be able
    // to see our lock and tweak our debugging counters, as it releases
    // and acquires locks inside of pthread_cond_{timed,}wait.
    // 
    // 条件变量类 ConditionVariable 的 posix 实现需要能够看到我们的锁并调整我们的
    // 互斥体计数器，因为它可能要先释放在获取锁 pthread_cond_{timed,}wait
friend class ConditionVariable;
#elif defined(OS_WIN)
// The Windows Vista implementation of ConditionVariable needs the
// native handle of the critical section.
// 
// ConditionVariable 的 Windows Vista 实现需要关键部分的本地句柄
friend class WinVistaCondVar;
#endif

    NativeHandle _native_handle;
};

// TODO: Remove this type.
// 
// Mutex 的包装器 Lock 类
class BUTIL_EXPORT Lock : public Mutex {
    DISALLOW_COPY_AND_ASSIGN(Lock);
public:
    Lock() {}
    ~Lock() {}
    void Acquire() { lock(); }
    void Release() { unlock(); }
    bool Try() { return try_lock(); }
    void AssertAcquired() const {}
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
// 
// Lock 的 RAII 包装器。自动锁定/解锁辅助类（构造时获取锁，析构时释放锁）
class AutoLock {
public:
    struct AlreadyAcquired {};

    explicit AutoLock(Lock& lock) : lock_(lock) {
        lock_.Acquire();
    }

    // 标示 lock 互斥体已经被锁定
    AutoLock(Lock& lock, const AlreadyAcquired&) : lock_(lock) {
        lock_.AssertAcquired();
    }

    ~AutoLock() {
        lock_.AssertAcquired();
        lock_.Release();
    }

private:
    Lock& lock_;
    DISALLOW_COPY_AND_ASSIGN(AutoLock);
};

// AutoUnlock is a helper that will Release() the |lock| argument in the
// constructor, and re-Acquire() it in the destructor.
// 
// AutoLock 相反操作（构造时释放锁，析构时获取锁）
class AutoUnlock {
public:
    explicit AutoUnlock(Lock& lock) : lock_(lock) {
        // We require our caller to have the lock.
        lock_.AssertAcquired();
        lock_.Release();
    }

    ~AutoUnlock() {
        lock_.Acquire();
    }

private:
    Lock& lock_;
    DISALLOW_COPY_AND_ASSIGN(AutoUnlock);
};

}  // namespace butil

#endif  // BUTIL_SYNCHRONIZATION_LOCK_H_

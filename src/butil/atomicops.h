// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For atomic operations on reference counts, see atomic_refcount.h.
// For atomic operations on sequence numbers, see atomic_sequence_num.h.

// The routines exported by this module are subtle.  If you use them, even if
// you get the code right, it will depend on careful reasoning about atomicity
// and memory ordering; it will be less readable, and harder to maintain.  If
// you plan to use these routines, you should have a good reason, such as solid
// evidence that performance would otherwise suffer, or there being no
// alternative.  You should assume only properties explicitly guaranteed by the
// specifications in this file.  You are almost certainly _not_ writing code
// just for the x86; if you assume x86 semantics, x86 hardware bugs and
// implementations on other archtectures will cause your code to break.  If you
// do not know what you are doing, avoid these routines, and use a Mutex.
//
// It is incorrect to make direct assignments to/from an atomic variable.
// You should use one of the Load or Store routines.  The NoBarrier
// versions are provided when no barriers are needed:
//   NoBarrier_Store()
//   NoBarrier_Load()
// Although there are currently no compiler enforcement, you are encouraged
// to use these.
//

// @tips
// 1. 内存屏障，其实本质上是为了解决因为编译器优化和 CPU 对 CPU Cache/寄存器(Register) 的
//    使用，从而导致对内存的操作不能够及时反映出来的一类问题。比如 CPU 写入后，读出来的值可能
//    是旧的内容。
// 2. 内存屏障，是一类同步屏障指令，是 CPU 或编译器在对内存随机访问操作中的一个同步点，使得此点
//    之前的所有读写操作都执行后才可以开始执行此点之后的操作。大多数现代计算机为了提高性能而采取
//    乱序执行，这使得内存屏障成为必须。
// 
// CPU 一般情况下是不直接读写内存的 (emmintrin.h 应用例外)，其一般流程为：
// 读取内存数据到 CPU Cache -->  CPU 读取 CPU Cache/寄存器(Register) -->  
// CPU 的计算(CPU ALU 单元) --> 将结果写入 CPU Cache /寄存器(Register) --> 写回数据到内存
// 
// 语义上，内存屏障之前的所有写操作都要写入内存；内存屏障之后的读操作都可以获得同步内存屏障之前的
// 写操作的结果。因此，对于敏感的程序块，写操作之后、读操作之前可以插入内存屏障。
// 
// 内存屏障提供了两种属性：
// 1. 它们保留了外部可见的程序顺序。通过确保所有的、屏障两侧的指令表现出正确的程序顺序，比如从其
//    他 CPU 观察。
// 2. 它们使内部可见。通过确保数据传播到缓存子系统。

#ifndef BUTIL_ATOMICOPS_H_
#define BUTIL_ATOMICOPS_H_

#include <stdint.h>

#include "butil/build_config.h"
#include "butil/macros.h"

#if defined(OS_WIN) && defined(ARCH_CPU_64_BITS)
// windows.h #defines this (only on x64). This causes problems because the
// public API also uses MemoryBarrier at the public name for this fence. So, on
// X64, undef it, and call its documented
// (http://msdn.microsoft.com/en-us/library/windows/desktop/ms684208.aspx)
// implementation directly.
#undef MemoryBarrier
#endif

namespace butil {
namespace subtle {

// 声明 32-bit 的 Atomic32 类型
typedef int32_t Atomic32;
// 64-bit 架构上定义 Atomic64 类型
#ifdef ARCH_CPU_64_BITS
// We need to be able to go between Atomic64 and AtomicWord implicitly.  This
// means Atomic64 and AtomicWord should be the same type on 64-bit.
// 
// 我们需要能够隐含地在 Atomic64 和 AtomicWord 之间转换。
// 这意味着 Atomic64 和 AtomicWord 应该是 64 位上相同的类型，都是 64-bit 的。
#if defined(__ILP32__) || defined(OS_NACL)
// NaCl's intptr_t is not actually 64-bits on 64-bit!
// http://code.google.com/p/nativeclient/issues/detail?id=1162
typedef int64_t Atomic64;
#else
typedef intptr_t Atomic64;
#endif
#endif

// Use AtomicWord for a machine-sized pointer.  It will use the Atomic32 or
// Atomic64 routines below, depending on your architecture.
// 
// 使用 AtomicWord 作为机器指针。依赖 CPU 架构平台，其可能使用 Atomic32/Atomic64
typedef intptr_t AtomicWord;

// Atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// Always return the old value of "*ptr"
//
// This routine implies no memory barriers.
// 
// 无内存屏障，原子的比较和交换操作。
// 如果 ptr 和 old_value 的值一样，把 new_value 写到 ptr 内存，返回 old_value 。否
// 则返回 ptr 的旧值。（返回值始终等于 ptr 的旧值）。
Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                  Atomic32 old_value,
                                  Atomic32 new_value);

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
// 
// 无内存屏障，原子的交换操作。
// 返回交换后的 new_value ：即返回 ptr 的旧值
Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr, Atomic32 new_value);

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
// 
// 无内存屏障，原子的自增操作。
// 返回自增后的 *ptr 的值
Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr, Atomic32 increment);

// 原子的自增（带屏障）。返回自增后的 *ptr 的值
Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                 Atomic32 increment);

// These following lower-level operations are typically useful only to people
// implementing higher-level synchronization operations like spinlocks,
// mutexes, and condition-variables.  They combine CompareAndSwap(), a load, or
// a store with appropriate memory-ordering instructions.  "Acquire" operations
// ensure that no later memory access can be reordered ahead of the operation.
// "Release" operations ensure that no previous memory access can be reordered
// after the operation.  "Barrier" operations have both "Acquire" and "Release"
// semantics.   A MemoryBarrier() has "Barrier" semantics, but does no memory
// access.
// 
// 以下较低级别的操作通常仅适用于实施更高级同步操作的人员，如自旋锁，互斥锁和条件变量。它们
// 将 CompareAndSwap() ，加载或存储与适当的内存排序指令结合在一起。 "Acquire" 操作可确
// 保在操作之前不会再重新排序内存访问。 "Release" 操作确保在操作之后不能重新排序先前的存储
// 器访问。 "Barrier" 操作同时具有 "Acquire" 和 "Release" 语义。 MemoryBarrier() 具
// 有 "Barrier" 语义，但没有内存访问权限
// 
// 原子的比较和交换操作
Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                Atomic32 old_value,
                                Atomic32 new_value);
// 原子的比较和交换操作
Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                Atomic32 old_value,
                                Atomic32 new_value);

void MemoryBarrier();
void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value);
void Acquire_Store(volatile Atomic32* ptr, Atomic32 value);
void Release_Store(volatile Atomic32* ptr, Atomic32 value);

Atomic32 NoBarrier_Load(volatile const Atomic32* ptr);
Atomic32 Acquire_Load(volatile const Atomic32* ptr);
Atomic32 Release_Load(volatile const Atomic32* ptr);

// 64-bit atomic operations (only available on 64-bit processors).
#ifdef ARCH_CPU_64_BITS
Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                  Atomic64 old_value,
                                  Atomic64 new_value);
Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr, Atomic64 new_value);
Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr, Atomic64 increment);
Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr, Atomic64 increment);

Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                Atomic64 old_value,
                                Atomic64 new_value);
Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                Atomic64 old_value,
                                Atomic64 new_value);
void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value);
void Acquire_Store(volatile Atomic64* ptr, Atomic64 value);
void Release_Store(volatile Atomic64* ptr, Atomic64 value);
Atomic64 NoBarrier_Load(volatile const Atomic64* ptr);
Atomic64 Acquire_Load(volatile const Atomic64* ptr);
Atomic64 Release_Load(volatile const Atomic64* ptr);
#endif  // ARCH_CPU_64_BITS

}  // namespace subtle
}  // namespace butil

// Include our platform specific implementation.
// 
// 包含平台相关原子操作具体实现
#if defined(THREAD_SANITIZER)
#include "butil/atomicops_internals_tsan.h"
#elif defined(OS_WIN) && defined(COMPILER_MSVC) && defined(ARCH_CPU_X86_FAMILY)
#include "butil/atomicops_internals_x86_msvc.h"
#elif defined(OS_MACOSX)
#include "butil/atomicops_internals_mac.h"
#elif defined(OS_NACL)
#include "butil/atomicops_internals_gcc.h"
#elif defined(COMPILER_GCC) && defined(ARCH_CPU_ARMEL)
#include "butil/atomicops_internals_arm_gcc.h"
#elif defined(COMPILER_GCC) && defined(ARCH_CPU_ARM64)
#include "butil/atomicops_internals_arm64_gcc.h"
#elif defined(COMPILER_GCC) && defined(ARCH_CPU_X86_FAMILY)
#include "butil/atomicops_internals_x86_gcc.h"
#elif defined(COMPILER_GCC) && defined(ARCH_CPU_MIPS_FAMILY)
#include "butil/atomicops_internals_mips_gcc.h"
#else
#error "Atomic operations are not supported on your platform"
#endif

// On some platforms we need additional declarations to make
// AtomicWord compatible with our other Atomic* types.
#if defined(OS_MACOSX) || defined(OS_OPENBSD)
#include "butil/atomicops_internals_atomicword_compat.h"
#endif

// ========= Provide butil::atomic<T> =========
#if defined(BUTIL_CXX11_ENABLED)

// gcc supports atomic thread fence since 4.8 checkout
// https://gcc.gnu.org/gcc-4.7/cxx0x_status.html and
// https://gcc.gnu.org/gcc-4.8/cxx0x_status.html for more details
// 
// gcc 4.8 版本检查
#if defined(__clang__) || \
    !defined(__GNUC__) || \
    (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 >= 40800)

// C++11 标准库原子类型
# include <atomic>

#else 

# if __GNUC__ * 10000 + __GNUC_MINOR__ * 100 >= 40500
// gcc 4.5 renames cstdatomic to atomic
// (https://gcc.gnu.org/gcc-4.5/changes.html)
#  include <atomic>
# else
// gcc 4.5 之前版本，原子类型标准库
#  include <cstdatomic>
# endif

// @todo
// 修改 std 命令空间（未定义行为）

namespace std {

BUTIL_FORCE_INLINE void atomic_thread_fence(memory_order v) {
    switch (v) {
    case memory_order_relaxed:
        break;
    case memory_order_consume:
    case memory_order_acquire:
    case memory_order_release:
    case memory_order_acq_rel:
        __asm__ __volatile__("" : : : "memory");
        break;
    case memory_order_seq_cst:
        __asm__ __volatile__("mfence" : : : "memory");
        break;
    }
}

BUTIL_FORCE_INLINE void atomic_signal_fence(memory_order v) {
    if (v != memory_order_relaxed) {
        __asm__ __volatile__("" : : : "memory");
    }
}

}  // namespace std

#endif  // __GNUC__

namespace butil {
using ::std::memory_order;
using ::std::memory_order_relaxed;
using ::std::memory_order_consume;
using ::std::memory_order_acquire;
using ::std::memory_order_release;
using ::std::memory_order_acq_rel;
using ::std::memory_order_seq_cst;
using ::std::atomic_thread_fence;
using ::std::atomic_signal_fence;
template <typename T> class atomic : public ::std::atomic<T> {
public:
    atomic() {}
    atomic(T v) : ::std::atomic<T>(v) {}
    atomic& operator=(T v) {
        this->store(v);
        return *this;
    }
private:
    DISALLOW_COPY_AND_ASSIGN(atomic);
    // Make sure memory layout of std::atomic<T> and boost::atomic<T>
    // are same so that different compilation units seeing different 
    // definitions(enable C++11 or not) should be compatible.
    BAIDU_CASSERT(sizeof(T) == sizeof(::std::atomic<T>), size_must_match);
};
} // namespace butil
#else
// C++11 之前使用 <boost/atomic.hpp>
#include <boost/atomic.hpp>
namespace butil {
using ::boost::memory_order;
using ::boost::memory_order_relaxed;
using ::boost::memory_order_consume;
using ::boost::memory_order_acquire;
using ::boost::memory_order_release;
using ::boost::memory_order_acq_rel;
using ::boost::memory_order_seq_cst;
using ::boost::atomic_thread_fence;
using ::boost::atomic_signal_fence;
template <typename T> class atomic : public ::boost::atomic<T> {
public:
    atomic() {}
    atomic(T v) : ::boost::atomic<T>(v) {}
    atomic& operator=(T v) {
        this->store(v);
        return *this;
    }
private:
    DISALLOW_COPY_AND_ASSIGN(atomic);
    // Make sure memory layout of std::atomic<T> and boost::atomic<T>
    // are same so that different compilation units seeing different 
    // definitions(enable C++11 or not) should be compatible.
    BAIDU_CASSERT(sizeof(T) == sizeof(::boost::atomic<T>), size_must_match);
};
} // namespace butil
#endif

// static_atomic<> is a work-around for C++03 to declare global atomics
// w/o constructing-order issues. It can also used in C++11 though.
// Example:
//   butil::static_atomic<int> g_counter = BUTIL_STATIC_ATOMIC_INIT(0);
// Notice that to make static_atomic work for C++03, it cannot be
// initialized by a constructor. Following code is wrong:
//   butil::static_atomic<int> g_counter(0); // Not compile

// 全局静态原子类型，也可运行在 C++11 上

#define BUTIL_STATIC_ATOMIC_INIT(val) { (val) }

namespace butil {
template <typename T> struct static_atomic {
    T val;

    // NOTE: the memory_order parameters must be present.
    T load(memory_order o) { return ref().load(o); }
    void store(T v, memory_order o) { return ref().store(v, o); }
    T exchange(T v, memory_order o) { return ref().exchange(v, o); }
    bool compare_exchange_weak(T& e, T d, memory_order o)
    { return ref().compare_exchange_weak(e, d, o); }
    bool compare_exchange_weak(T& e, T d, memory_order so, memory_order fo)
    { return ref().compare_exchange_weak(e, d, so, fo); }
    bool compare_exchange_strong(T& e, T d, memory_order o)
    { return ref().compare_exchange_strong(e, d, o); }
    bool compare_exchange_strong(T& e, T d, memory_order so, memory_order fo)
    { return ref().compare_exchange_strong(e, d, so, fo); }
    T fetch_add(T v, memory_order o) { return ref().fetch_add(v, o); }
    T fetch_sub(T v, memory_order o) { return ref().fetch_sub(v, o); }
    T fetch_and(T v, memory_order o) { return ref().fetch_and(v, o); }
    T fetch_or(T v, memory_order o) { return ref().fetch_or(v, o); }
    T fetch_xor(T v, memory_order o) { return ref().fetch_xor(v, o); }
    static_atomic& operator=(T v) {
        store(v, memory_order_seq_cst);
        return *this;
    }
private:
    DISALLOW_ASSIGN(static_atomic);
    BAIDU_CASSERT(sizeof(T) == sizeof(atomic<T>), size_must_match);
    atomic<T>& ref() {
        // Suppress strict-alias warnings.
        atomic<T>* p = reinterpret_cast<atomic<T>*>(&val);
        return *p;
    }
};
} // namespace butil

#endif  // BUTIL_ATOMICOPS_H_

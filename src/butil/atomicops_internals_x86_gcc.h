// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use butil/atomicops.h instead.
// 
// 这个文件是 x86 平台原子操作内部实现，外部请使用 butil/atomicops.h

// @tips
// 一. Compiler barrier
// ----------------------------------------------------------------------------------
// #define mfence_c() __asm__ __volatile__("" : : : "memory")
// #define barrier() __asm__ __volatile__("" : : : "memory")
// 
// #include <cstdio>
// int a = 0;
// int main(...) {
//   printf("%p, %d\n", &a, a);
//   
//   *(int*)(0x5009ac) = 1; // 假设我们知道 &a 的地址就是 0x5009ac
//   
//   printf("%p, %d\n", &a, a);
//   return 0;
// }
// -----------------------------------------------------------------------------------
// 这段代码，如果使用 -O2 编译的话，那么可能会发现 a 没有修改。因为：
// 1. 编译器在生成执行指令的时候，将 a 存放在寄存器里面。而在进行赋值操作的时候，gcc 并没有认为
// 这里是修改了 a(gcc 并不知道那个地址就是 a)，下次再读取 a 的时候依然是读寄存器的内容，而不是
// 我们修改的内容。所以我们必须显式地告诉 gcc ，我这个操作可能会修改变量内容，之后你需要重新从内
// 存读取。
// 2. 这条语句可能实际上并不生成任何代码，但可使 gcc 在该代码之后刷新寄存器对变量的缓存。
// 3. barrier() 除了上面功能（强迫编译器刷新寄存器分配）之外，它还会阻止 gcc 进行 instruction 
// reorder（指令重排）而出现的问题。
// 
// 注意： barrier() 和关键字 volatile 是存在差别的，volatile 只是告诉编译器说：我的值不要存放在
// 寄存器，所有对于我的读写都是直接操作内存。他并不能阻止 instruction reorder（指令重排）。
// 
// 事实上，上面这种情况是可以通过 volatile 来解决的。volatile 意思就是易变性，也就是说这个变量可能
// 以某种编译器 (compiler) 未知的方式被修改掉。 volatile 这个关键字纯粹是为了编译器优化而存在的，
// 编译器优化将内存操作映射到寄存器操作为了加快速度，但是某些情况下面我们就是希望真切地读取内存而非寄
// 存器。正是因为存在优化编译器，所以我们需要 volatile. 它多用于信号处理函数里读取全局变量。
// 
// 二. CPU barrier
// #define rmb() __asm__ __volatile__("lfence" : : : "memory")
// #define wmb() __asm__ __volatile__("sfence" : : : "memory")
// #define mb() __asm__ __volatile__("mfence" : : : "memory")
// 
// 1. Store barrier（写屏障），是 x86 上的 "sfence" 指令：强制所有在 sfence 指令之前的 store 指
// 令，都在该 sfence 指令执行之前被执行，并让 store buffer 的数据都刷新到发布这个指令的 cpucache
// 即：程序要想越过写屏障继续执行，必须先处理写屏障之前的所有写指令，让其全部先执行完成。
// 
// 2. Load barrier（读屏障），是 x86 上的 "lfence" 指令：强制所有在 lfence 指令之后的 load 指
// 令，都在该 lfence 指令执行生效之前被执行。即处理器会一直等到 load buffer 被排空后再继续执行。
// 即：程序要想越过读屏障继续执行，必须先处理在读屏障之前但又会影响读屏障之后的所有的读指令，让其全部
// 先执行完成。所以它配合 sfence 指令，能做到即时跨 CPU 也能起到保证在 sfence 承诺的指令执行顺序！
// 
// 3. Full barrier（读/写屏障），是 x86 上的 "mfence" 指令，复合了 load 和 Store 屏障的功能。
// 
// 从中可以看出 Load barrier 与 Store barrier 应该成对使用。
// 说明如下：
// --------------------------------------------------------------------------------------
// 假设场景：
// # CPU0 执行代码
// a = 1;
// wmb(); // 写内存屏障  // 可能要与所有 CPU 通信
// b = 1;
// 
// # CPU1 执行代码
// while (b == 0) {};
// assert(a == 1);
// ---------------------------------------------------------------------------------------
// 上面的代码片段中，我们假设 a 和 b 初值是 0. a 在 CPU0 和 CPU1 都有缓存，a 变量对应的 CPU0 和 CPU1 
// 的 cacheline 是 shared 状态，b 处于 exclusive 状态，被 CPU0 独占。（详情查看 MSEI 一致性协议）
// 
// 执行详细过程：
// 1. CPU0 执行 a=1 的赋值操作。由于 a 在 CPU0 local cache 中的 cacheline 处于 shared 状态，因此 
//    CPU0 将 a 的新值 "1" 放入 store buffer，并且发送了 invalidate 消息去清空其他 CPU(CPU1) 对应
//    的 cacheline.
// 2. CPU1 执行 while(b == 0) 的循环操作，但是 b 没有在 CPU1 local cache ，因此发送 read 消息试图
//    获取该值。
// 3. CPU1 收到了 CPU0 的 invalidate 消息，放入 Invalidate Queue ，并立刻回送 Ack.
// 4. CPU0 收到了 CPU1 的 invalidate ACK 之后，即可以越过程序设定内存屏障（第二行代码的 wmb() ），这
//    样 a 的新值从 store buffer 进入 CPU0 cacheline ，状态变成 Modified.
// 5. CPU0 越过 memory barrier 后继续执行 b=1 的赋值操作。由于 b 值在 CPU0 local cache 中（并且是 
//    exclusive 状态），因此 store 操作完成并进入 cacheline.
// 6. CPU0 收到了 read 消息后将 b 的最新值 "1" 回送给 CPU1 ，并修正 CPU0 cacheline 为 shared 状态
// 7. CPU1 收到 read response ，将 b 的最新值 "1" 加载到 local cacheline ，设置状态为 shared.
// 8. CPU1 而言，b 已经等于 1 了，因此跳出 while(b == 0) 的循环，继续执行后续代码。
// 9. CPU1 在执行 assert(a == 1) 时候，由于这时候 CPU1 cache 的 a 值仍然是旧值 0，因此 assertion 失败
// 10.该来总会来，Invalidate Queue 中针对 a cacheline 的 invalidate 消息最终会被 CPU1 执行，将 a 设定
//    为无效，但是大错已经酿成。
// 
// 修改 CPU1 代码
// while (b == 0) {};
// rmb();        //（读内存屏障）  // 清空处理 CPU 所有 load buffer 消息
// assert(a == 1);
// 
// 执行详细过程（ 1~8 不变）：
// 9. CPU1 现在不能继续执行代码，只能等待，直到 Invalidate Queue 中的 message 被处理完成。
// 10.CPU1 处理队列中缓存的 Invalidate 消息，将 a 对应的 cacheline 设置为无效。
// 11.由于 a 变量在 local cache 中无效，因此 CPU1 在执行 assert(a == 1) 的时候需要发送一个 read 消
//    息去获取 a 值。
// 12.CPU0 用 a 的新值 "1" 回应来自 CPU1 的请求。
// 13.CPU1 获得了 a 的新值，并放入 CPU1 cacheline ，这时候 assert(a == 1) 不会失败了。

#ifndef BUTIL_ATOMICOPS_INTERNALS_X86_GCC_H_
#define BUTIL_ATOMICOPS_INTERNALS_X86_GCC_H_

#include "butil/base_export.h"

// This struct is not part of the public API of this module; clients may not
// use it.  (However, it's exported via BUTIL_EXPORT because clients implicitly
// do use it at link time by inlining these functions.)
// Features of this x86.  Values may not be correct before main() is run,
// but are set conservatively.
// 
// 此结构体不是此模块的公共 API 的一部分; 客户可能不会使用它（它通过 BASE_EXPORT 导出，
// 客户端可以通过内联这些函数，编译器可以在链接时隐含地使用它）
// 
// x86 的特性，在运行 main() 之前，值可能不正确，但是保守设置。
struct AtomicOps_x86CPUFeatureStruct {
  bool has_amd_lock_mb_bug; // Processor has AMD memory-barrier bug; do lfence
                            // after acquire compare-and-swap.
};
BUTIL_EXPORT extern struct AtomicOps_x86CPUFeatureStruct
    AtomicOps_Internalx86CPUFeatures;

// 编译器屏障
#define ATOMICOPS_COMPILER_BARRIER() __asm__ __volatile__("" : : : "memory")

namespace butil {
namespace subtle {

// @tips
// C 语言内嵌汇编格式是：
// asm(assembler template  
// : output operands            (optional)
// : input operands             (optional)
// : clobbered registers list   (optional)
// );
// // output operands 和 inpupt operands 指定参数，它们从左到右依次排列，用 ',' 分割，
// 编号从 0 开始。
// 
// 
// 以 Linux Kernel 中的 cmpxchg 汇编为例 <include/asm-i386/cmpxchg.h>
// #define cmpxchg(ptr, _old, _new) { \
//   volatile uint32_t *__ptr = (volatile uint32_t *)(ptr); \
//   uint32_t __ret;                                \
//   asm volatile("lock; cmpxchgl %2,%1"            \
//     : "=a" (__ret), "+m" (*__ptr)                \
//     : "r" (_new), "0" (_old)                     \
//     : "memory");                                 \
//   );                                             \
//   __ret;                                         \
// }
// 1. (__ret) 对应 0，(*__ptr) 对应 1，(_new) 对应 2，(_old) 对应 3，如果在汇编中用到 "%2",
//    那么就是指代 _new，"%1" 指代 (*__ptr)
// 2. "=a" 是说要把结果写到 __ret 中，而且要使用 eax 寄存器，所以最后写结果的时候是的操作指令是 
//    mov eax, ret (eax==>__ret)
// 3. "r" (_new) 是要把 _new 的值读到一个通用寄存器中使用
// 4. "0"(_old) ，它像告诉你 (_old) 和第 0 号操作数使用相同的寄存器或者内存，即 (_old) 的存储
//    在和 0 号操作数一样的地方。就是说 _old 和 __ret 使用一样的寄存器，而 __ret 使用的寄存器是
//    eax，所以 _old 也用 eax
// 
// cmpxchgl 指令：比较 eax 和目的操作数(第一个操作数)的值，如果相同，ZF 标志被设置，同时源操作数
// (第二个操作)的值被写到目的操作数，否则，清 ZF 标志，并且把目的操作数的值写回 eax
// 
// cmpxchg 宏：比较 _old 和 (*__ptr) 的值，如果相同，ZF 标志被设置，同时 _new 的值被写到 (*__ptr)，
// 否则，清 ZF 标志，并且把 (*__ptr) 的值写回 _old
// 
// 另：Intel 开发手册上说 lock 就是让 CPU 排他地使用内存。
// 

// 32-bit low-level operations on any platform.

// 无内存屏障，原子的比较和交换操作。
// 如果 ptr 和 old_value 的值一样，把 new_value 写到 ptr 内存，返回 old_value 。否则返回 ptr 
// 的值。（返回值始终等于 ptr 的旧值）。
inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  Atomic32 prev;
  __asm__ __volatile__("lock; cmpxchgl %1,%2"
                       : "=a" (prev)
                       : "q" (new_value), "m" (*ptr), "0" (old_value)
                       : "memory");
  return prev;
}

// 无内存屏障，原子的交换操作。
// 返回交换后的 new_value ：即返回 ptr 的旧值
inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  __asm__ __volatile__("xchgl %1,%0"  // The lock prefix is implicit for xchg.
                       : "=r" (new_value)
                       : "m" (*ptr), "0" (new_value)
                       : "memory");
  return new_value;  // Now it's the previous value.
}

// 无内存屏障，原子的自增操作。
// 交换第一个操作数（目标操作数）与第二个操作数（源操作数），然后将这两个值的和加载到目标操作数。
// 目标操作数可以是寄存器或内存位置；源操作数是寄存器。
inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  Atomic32 temp = increment;
  __asm__ __volatile__("lock; xaddl %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now holds the old value of *ptr
  // 
  // 此时， tmp 持有 *ptr 的旧值
  return temp + increment;
}

// 原子的自增（带屏障）。返回自增后的 *ptr 的值
inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  Atomic32 temp = increment;
  __asm__ __volatile__("lock; xaddl %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now holds the old value of *ptr
  // 
  // 此时， tmp 持有 *ptr 的旧值
  if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug) {
    // 设置读屏障
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return temp + increment;
}

// 以下较低级别的操作通常仅适用于实施更高级同步操作的人员，如自旋锁，互斥锁和条件变量。它们
// 将 CompareAndSwap() ，加载或存储与适当的内存排序指令结合在一起。 "Acquire" 操作可确
// 保在操作之前不会再重新排序内存访问。 "Release" 操作确保在操作之后不能重新排序先前的存储
// 器访问。 "Barrier" 操作同时具有 "Acquire" 和 "Release" 语义。 MemoryBarrier() 具
// 有 "Barrier" 语义，但没有内存访问权限

// 原子的比较和交换操作（带屏障）。
inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  Atomic32 x = NoBarrier_CompareAndSwap(ptr, old_value, new_value);
  if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug) {
    // 设置读屏障
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return x;
}

// 原子的比较和交换操作（相比 Acquire_CompareAndSwap 不带屏障）
inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}


// Acquire_Store|Release_Load 配对使用，两者都附带有 CPU 读/写内存屏障
// Release_Store|Acquire_Load 配对使用，两者都附带有 编译器内存屏障

// 非原子存储操作
inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
}

// CPU 读/写内存屏障
inline void MemoryBarrier() {
  __asm__ __volatile__("mfence" : : : "memory");
}

// 非原子存储操作（先写，再设置 CPU 读/写内存屏障）
inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
  MemoryBarrier();
}

// 非原子存储操作（先设置编译器内存屏障，再写）
inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  ATOMICOPS_COMPILER_BARRIER();
  *ptr = value; // An x86 store acts as a release barrier.
  // See comments in Atomic64 version of Release_Store(), below.
}

// 非原子读取操作
inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return *ptr;
}

// 非原子读取操作（先读，再带编译器内存屏障）
inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  Atomic32 value = *ptr; // An x86 load acts as a acquire barrier.
  // See comments in Atomic64 version of Release_Store(), below.
  ATOMICOPS_COMPILER_BARRIER();
  return value;
}

// 非原子读取操作（先设置 CPU 读写内存屏障，再读）
inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  MemoryBarrier();
  return *ptr;
}

#if defined(__x86_64__)

// 64-bit low-level operations on 64-bit platform.

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  Atomic64 prev;
  __asm__ __volatile__("lock; cmpxchgq %1,%2"
                       : "=a" (prev)
                       : "q" (new_value), "m" (*ptr), "0" (old_value)
                       : "memory");
  return prev;
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  __asm__ __volatile__("xchgq %1,%0"  // The lock prefix is implicit for xchg.
                       : "=r" (new_value)
                       : "m" (*ptr), "0" (new_value)
                       : "memory");
  return new_value;  // Now it's the previous value.
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  Atomic64 temp = increment;
  __asm__ __volatile__("lock; xaddq %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now contains the previous value of *ptr
  return temp + increment;
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  Atomic64 temp = increment;
  __asm__ __volatile__("lock; xaddq %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now contains the previous value of *ptr
  if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return temp + increment;
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
  MemoryBarrier();
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  ATOMICOPS_COMPILER_BARRIER();

  *ptr = value; // An x86 store acts as a release barrier
                // for current AMD/Intel chips as of Jan 2008.
                // See also Acquire_Load(), below.

  // When new chips come out, check:
  //  IA-32 Intel Architecture Software Developer's Manual, Volume 3:
  //  System Programming Guide, Chatper 7: Multiple-processor management,
  //  Section 7.2, Memory Ordering.
  // Last seen at:
  //   http://developer.intel.com/design/pentium4/manuals/index_new.htm
  //
  // x86 stores/loads fail to act as barriers for a few instructions (clflush
  // maskmovdqu maskmovq movntdq movnti movntpd movntps movntq) but these are
  // not generated by the compiler, and are rare.  Users of these instructions
  // need to know about cache behaviour in any case since all of these involve
  // either flushing cache lines or non-temporal cache hints.
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return *ptr;
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  Atomic64 value = *ptr; // An x86 load acts as a acquire barrier,
                         // for current AMD/Intel chips as of Jan 2008.
                         // See also Release_Store(), above.
  ATOMICOPS_COMPILER_BARRIER();
  return value;
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  MemoryBarrier();
  return *ptr;
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  Atomic64 x = NoBarrier_CompareAndSwap(ptr, old_value, new_value);
  if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return x;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

#endif  // defined(__x86_64__)

} // namespace butil::subtle
} // namespace butil

#undef ATOMICOPS_COMPILER_BARRIER

#endif  // BUTIL_ATOMICOPS_INTERNALS_X86_GCC_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a low level implementation of atomic semantics for reference
// counting.  Please use butil/memory/ref_counted.h directly instead.
//
// The implementation includes annotations to avoid some false positives
// when using data race detection tools.
// 
// 用于引用计数的原子语义的底层实现。实现包含注释，避免在使用数据条件竞争检测工具时出现
// 的不正确的 false 返回。
// 引用计数器请直接使用 butil/memory/ref_counted.h

#ifndef BUTIL_ATOMIC_REF_COUNT_H_
#define BUTIL_ATOMIC_REF_COUNT_H_

#include "butil/atomicops.h"
#include "butil/third_party/dynamic_annotations/dynamic_annotations.h"

namespace butil {

typedef subtle::Atomic32 AtomicRefCount;

// Increment a reference count by "increment", which must exceed 0.
// 
// 原子增加 |ptr| 引用计数 |increment| (该值必须超过 0)。
inline void AtomicRefCountIncN(volatile AtomicRefCount *ptr,
                               AtomicRefCount increment) {
  subtle::NoBarrier_AtomicIncrement(ptr, increment);
}

// Decrement a reference count by "decrement", which must exceed 0,
// and return whether the result is non-zero.
// Insert barriers to ensure that state written before the reference count
// became zero will be visible to a thread that has just made the count zero.
// 
// 原子减少 |ptr| 引用计数 |decrement| (该值必须超过 0)。并返回结果是否为非零。
// 插入障碍以确保在引用计数变为零之前写入的状态对于刚刚使计数为零的线程可见。
inline bool AtomicRefCountDecN(volatile AtomicRefCount *ptr,
                               AtomicRefCount decrement) {
  ANNOTATE_HAPPENS_BEFORE(ptr);
  bool res = (subtle::Barrier_AtomicIncrement(ptr, -decrement) != 0);
  if (!res) {
    ANNOTATE_HAPPENS_AFTER(ptr);
  }
  return res;
}

// Increment a reference count by 1.
// 
// 原子增加 |ptr| 引用计数 1
inline void AtomicRefCountInc(volatile AtomicRefCount *ptr) {
  butil::AtomicRefCountIncN(ptr, 1);
}

// Decrement a reference count by 1 and return whether the result is non-zero.
// Insert barriers to ensure that state written before the reference count
// became zero will be visible to a thread that has just made the count zero.
// 
// 原子减少 |ptr| 引用计数 1
inline bool AtomicRefCountDec(volatile AtomicRefCount *ptr) {
  return butil::AtomicRefCountDecN(ptr, 1);
}

// Return whether the reference count is one.  If the reference count is used
// in the conventional way, a refrerence count of 1 implies that the current
// thread owns the reference and no other thread shares it.  This call performs
// the test for a reference count of one, and performs the memory barrier
// needed for the owning thread to act on the object, knowing that it has
// exclusive access to the object.
// 
// 返回引用计数是否为 1 。如果以常规方式使用引用计数，则返回计数为 1 意味着当前线程拥有引用，
// 而没有其他线程共享它。该调用执行一个引用计数的测试，并执行所拥有的线程对对象所需的内存屏障，
// 直到它具有对对象的独占访问权限。
inline bool AtomicRefCountIsOne(volatile AtomicRefCount *ptr) {
  bool res = (subtle::Acquire_Load(ptr) == 1);
  if (res) {
    ANNOTATE_HAPPENS_AFTER(ptr);
  }
  return res;
}

// Return whether the reference count is zero.  With conventional object
// referencing counting, the object will be destroyed, so the reference count
// should never be zero.  Hence this is generally used for a debug check.
// 
// 返回引用计数是否为零。使用常规的对象引用计数，对象将被销毁，因此引用计数不应该为零。
// 因此，这通常用于调试检查。
inline bool AtomicRefCountIsZero(volatile AtomicRefCount *ptr) {
  bool res = (subtle::Acquire_Load(ptr) == 0);
  if (res) {
    ANNOTATE_HAPPENS_AFTER(ptr);
  }
  return res;
}

}  // namespace butil

#endif  // BUTIL_ATOMIC_REF_COUNT_H_

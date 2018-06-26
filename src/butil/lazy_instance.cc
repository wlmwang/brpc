// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/lazy_instance.h"

#include "butil/at_exit.h"
#include "butil/atomicops.h"
#include "butil/basictypes.h"
#include "butil/threading/platform_thread.h"
#include "butil/third_party/dynamic_annotations/dynamic_annotations.h"

namespace butil {
namespace internal {

// TODO(joth): This function could be shared with Singleton, in place of its
// WaitForInstance() call.
// 
// 这个函数可以与 Singleton 共享，代替其 WaitForInstance() 调用。
// 
// 检查本线程是否需要创建实例。如果是，则返回 true ，否则如果另一个线程正在创建实例，则
// 等待其创建完成并返回 false
bool NeedsLazyInstance(subtle::AtomicWord* state) {
  // Try to create the instance, if we're the first, will go from 0 to
  // kLazyInstanceStateCreating, otherwise we've already been beaten here.
  // The memory access has no memory ordering as state 0 and
  // kLazyInstanceStateCreating have no associated data (memory barriers are
  // all about ordering of memory accesses to *associated* data).
  // 
  // 尝试创建实例，如果该线程是第一个，将从 0 到 kLazyInstanceStateCreating，否则有
  // 其他线程正在创建。
  // 内存访问没有内存排序，因为状态为 0，而 kLazyInstanceStateCreating 没有关联数据
  // （内存屏障全部是关于对 *associated* 数据的内存访问顺序）。
  if (subtle::NoBarrier_CompareAndSwap(state, 0,
                                       kLazyInstanceStateCreating) == 0)
    // Caller must create instance
    // 
    // 调用者复制创建实例
    return true;

  // It's either in the process of being created, or already created. Spin.
  // The load has acquire memory ordering as a thread which sees
  // state_ == STATE_CREATED needs to acquire visibility over
  // the associated data (buf_). Pairing Release_Store is in
  // CompleteLazyInstance().
  // 
  // 实例正处于创建或已创建的过程中，旋转等待 Acquire_Load() 读取 state ，配对在 
  // CompleteLazyInstance() 中的 Release_Store() 完成内存读写顺序执行一致性。
  while (subtle::Acquire_Load(state) == kLazyInstanceStateCreating) {
    PlatformThread::YieldCurrentThread();
  }
  // Someone else created the instance.
  // 
  // 其他线程已创建实例成功
  return false;
}

void CompleteLazyInstance(subtle::AtomicWord* state,
                          subtle::AtomicWord new_instance,
                          void* lazy_instance,
                          void (*dtor)(void*)) {
  // See the comment to the corresponding HAPPENS_AFTER in Pointer().
  ANNOTATE_HAPPENS_BEFORE(state);

  // Instance is created, go from CREATING to CREATED.
  // Releases visibility over private_buf_ to readers. Pairing Acquire_Load's
  // are in NeedsInstance() and Pointer().
  // 
  // 通过对 state 的 Release_Store() 操作，向其他读线程发布写操作可见性（配
  // 合 NeedsInstance/Pointer 中 Acquire_Load() 完成内存读写顺序执行一致性 ）。
  subtle::Release_Store(state, new_instance);

  // Make sure that the lazily instantiated object will get destroyed at exit.
  // 
  // 确保在退出时延迟实例化的对象被销毁
  if (dtor)
    AtExitManager::RegisterCallback(dtor, lazy_instance);
}

}  // namespace internal
}  // namespace butil

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/memory/singleton.h"
#include "butil/threading/platform_thread.h"

namespace butil {
namespace internal {

// 线程间自旋等待实例创建完成。
subtle::AtomicWord WaitForInstance(subtle::AtomicWord* instance) {
  // Handle the race. Another thread beat us and either:
  // - Has the object in BeingCreated state
  // - Already has the object created...
  // We know value != NULL.  It could be kBeingCreatedMarker, or a valid ptr.
  // Unless your constructor can be very time consuming, it is very unlikely
  // to hit this race.  When it does, we just spin and yield the thread until
  // the object has been created.
  // 
  // 当 value != NULL ，它可能是 kBeingCreatedMarker 或有效的 ptr 。除非你的构造函
  // 数非常耗时，否则不太可能发生这种情况。当它发生时，我们只是旋转线程，直到创建对象完成。
  subtle::AtomicWord value;
  while (true) {
    // The load has acquire memory ordering as the thread which reads the
    // instance pointer must acquire visibility over the associated data.
    // The pairing Release_Store operation is in Singleton::get().
    // 
    // 读取实例指针的线程必须获取相关数据的可见性。配对 Release_Store 操作在 Singleton::get()
    value = subtle::Acquire_Load(instance);
    if (value != kBeingCreatedMarker)
      break;
    // sched_yield() 主动让出处理器。可以使另一个级别等于或高于当前线程的线程先运行。如果
    // 没有符合条件的线程，那么函数将会立刻返回然后继续执行当前线程的程序。
    PlatformThread::YieldCurrentThread();
  }
  return value;
}

}  // namespace internal
}  // namespace butil

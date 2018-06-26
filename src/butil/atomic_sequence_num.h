// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 原子序列类型。即，如果要在栈（或堆）上分配原子序列号 (atomic sequence number) ，
// 请使用 AtomicSequenceNumber 类。

#ifndef BUTIL_ATOMIC_SEQUENCE_NUM_H_
#define BUTIL_ATOMIC_SEQUENCE_NUM_H_

#include "butil/atomicops.h"
#include "butil/basictypes.h"

namespace butil {

class AtomicSequenceNumber;

// Static (POD) AtomicSequenceNumber that MUST be used in global scope (or
// non-function scope) ONLY. This implementation does not generate any static
// initializer.  Note that it does not implement any constructor which means
// that its fields are not initialized except when it is stored in the global
// data section (.data in ELF). If you want to allocate an atomic sequence
// number on the stack (or heap), please use the AtomicSequenceNumber class
// declared below.
// 
// 必须在全局域（或非函数域）中使用的静态 POD（ Static POD）的 AtomicSequenceNumber 。
// 此实现不会生成任何静态初始化程序。 
// 注意，它没有任何构造函数，这意味着它的字段不被初始化，除非它被存储在全局数据部分（ELF 中
// 的 .data）中（初始化为零值）。
// 如果要在栈（或堆）上分配序列号 (atomic sequence number)，请使用 AtomicSequenceNumber
class StaticAtomicSequenceNumber {
 public:
  inline int GetNext() {
    return static_cast<int>(
        butil::subtle::NoBarrier_AtomicIncrement(&seq_, 1) - 1);
  }

 private:
  friend class AtomicSequenceNumber;

  // 重置为 0
  inline void Reset() {
    butil::subtle::Release_Store(&seq_, 0);
  }

  butil::subtle::Atomic32 seq_;
};

// AtomicSequenceNumber that can be stored and used safely (i.e. its fields are
// always initialized as opposed to StaticAtomicSequenceNumber declared above).
// Please use StaticAtomicSequenceNumber if you want to declare an atomic
// sequence number in the global scope.
// 
// 可以安全地存储和使用 AtomicSequenceNumber（即，它的字段总是被初始化，而不是像上面依赖静
// 态储存区的初始化机制而声明的 StaticAtomicSequenceNumber ）。注：该类不是 POD 类型。
class AtomicSequenceNumber {
 public:
  AtomicSequenceNumber() {
    seq_.Reset();
  }

  inline int GetNext() {
    return seq_.GetNext();
  }

 private:
  StaticAtomicSequenceNumber seq_;
  DISALLOW_COPY_AND_ASSIGN(AtomicSequenceNumber);
};

}  // namespace butil

#endif  // BUTIL_ATOMIC_SEQUENCE_NUM_H_

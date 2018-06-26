// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/memory/weak_ptr.h"

namespace butil {
namespace internal {

WeakReference::Flag::Flag() : is_valid_(true) {
}

void WeakReference::Flag::Invalidate() {
  is_valid_ = false;
}

bool WeakReference::Flag::IsValid() const {
  return is_valid_;
}

WeakReference::Flag::~Flag() {
}

WeakReference::WeakReference() {
}

// 此时 flag_ 引用计数器 +1 ，即 flag_::ref_count_>=1
WeakReference::WeakReference(const Flag* flag) : flag_(flag) {
}

WeakReference::~WeakReference() {
}

bool WeakReference::is_valid() const { return flag_.get() && flag_->IsValid(); }

WeakReferenceOwner::WeakReferenceOwner() {
}

WeakReferenceOwner::~WeakReferenceOwner() {
  Invalidate();
}

WeakReference WeakReferenceOwner::GetRef() const {
  // If we hold the last reference to the Flag then create a new one.
  // 
  // 如果我们持有对 WeakReference::Flag 的最后一个引用，则创建一个 WeakReference
  if (!HasRefs())
    flag_ = new WeakReference::Flag();

  // 此时 flag_ 引用计数器 +1 ，即 flag_::ref_count_>=1
  return WeakReference(flag_.get());
}

void WeakReferenceOwner::Invalidate() {
  if (flag_.get()) {
    flag_->Invalidate();
    flag_ = NULL;
  }
}

WeakPtrBase::WeakPtrBase() {
}

WeakPtrBase::~WeakPtrBase() {
}

WeakPtrBase::WeakPtrBase(const WeakReference& ref) : ref_(ref) {
}

}  // namespace internal
}  // namespace butil

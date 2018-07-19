// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_SCOPED_OBSERVER_H_
#define BUTIL_SCOPED_OBSERVER_H_

#include <algorithm>
#include <vector>

#include "butil/basictypes.h"

// ScopedObserver is used to keep track of the set of sources an object has
// attached itself to as an observer. When ScopedObserver is destroyed it
// removes the object as an observer from all sources it has been added to.
// 
// ScopedObserver 用于 Observer 跟踪观察 Source 对象"事件变化"的自动管理类。作为
// 观察者 |observer| 会被加入到被观察者 |source| 的观察者列表中。当 ScopedObserver 
// 销毁时，观察者 |observer| 会从所有被观察者 |sources_| 列表中移除。
template <class Source, class Observer>
class ScopedObserver {
 public:
  explicit ScopedObserver(Observer* observer) : observer_(observer) {}

  ~ScopedObserver() {
    RemoveAll();
  }

  // Adds the object passed to the constructor as an observer on |source|.
  // 
  // 将当前观察者 |observer| 加入到被观察者 |source| 的观察者列表中。
  void Add(Source* source) {
    sources_.push_back(source);
    source->AddObserver(observer_);
  }

  // Remove the object passed to the constructor as an observer from |source|.
  // 
  // 将当前观察者 |observer| 从被观察者 |source| 的观察者列表中移除。
  void Remove(Source* source) {
    sources_.erase(std::find(sources_.begin(), sources_.end(), source));
    source->RemoveObserver(observer_);
  }

  void RemoveAll() {
    // 从被观察者列表中移除当前观察者对象
    for (size_t i = 0; i < sources_.size(); ++i)
      sources_[i]->RemoveObserver(observer_);
    sources_.clear();
  }

  bool IsObserving(Source* source) const {
    return std::find(sources_.begin(), sources_.end(), source) !=
        sources_.end();
  }

 private:
  // 当前观察者对象
  Observer* observer_;

  // 被观察者列表。即， observer_ 正在观察的所有对象列表
  std::vector<Source*> sources_;

  DISALLOW_COPY_AND_ASSIGN(ScopedObserver);
};

#endif  // BUTIL_SCOPED_OBSERVER_H_

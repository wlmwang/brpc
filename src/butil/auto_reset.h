// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_AUTO_RESET_H_
#define BUTIL_AUTO_RESET_H_

#include "butil/basictypes.h"

// butil::AutoReset<> is useful for setting a variable to a new value only within
// a particular scope. An butil::AutoReset<> object resets a variable to its
// original value upon destruction, making it an alternative to writing
// "var = false;" or "var = old_val;" at all of a block's exit points.
//
// This should be obvious, but note that an butil::AutoReset<> instance should
// have a shorter lifetime than its scoped_variable, to prevent invalid memory
// writes when the butil::AutoReset<> object is destroyed.
// 
// butil::AutoReset<> 主要用在：在特定域中将变量设为新值，离开特定域后，将值设回原值的场景。
// butil::AutoReset<> 对象在构造时设为新值，析构时将变量重置为原始值。
//
// 请注意，一个 butil::AutoReset<> 实例的生命周期应短于其 scoped_variable ，以防止在 
// butil::AutoReset<> 对象被销毁后的仍进行无效内存的写入。

namespace butil {

template<typename T>
class AutoReset {
 public:
  AutoReset(T* scoped_variable, T new_value)
      : scoped_variable_(scoped_variable),
        original_value_(*scoped_variable) {
    *scoped_variable_ = new_value;
  }

  ~AutoReset() { *scoped_variable_ = original_value_; }

 private:
  T* scoped_variable_;	// 特定域中设置值
  T original_value_;	// 原始值

  DISALLOW_COPY_AND_ASSIGN(AutoReset);
};

}

#endif  // BUTIL_AUTO_RESET_H_

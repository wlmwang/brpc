// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/at_exit.h"

#include <stddef.h>
#include <ostream>

#include "butil/logging.h"

namespace butil {

// Keep a stack of registered AtExitManagers.  We always operate on the most
// recent, and we should never have more than one outside of testing (for a
// statically linked version of this library).  Testing may use the shadow
// version of the constructor, and if we are building a dynamic library we may
// end up with multiple AtExitManagers on the same process.  We don't protect
// this for thread-safe access, since it will only be modified in testing.
// 
// 全局、静态的 AtExitManager 对象的指针以及执行流说明：
// 1. g_top_manager 每次都会被新声明的 AtExitManager 对象重新指向。重新该指针之前，让 
//    AtExitManager::next_manager_ 先保存 g_top_manager 指针，用于复原。
// 2. 若该新声明的 AtExitManager 对象超过作用域时，会随即自动执行在此期间注册的回调函数。
// 3. 更新 g_top_manager 重置回先前保存在 AtExitManager::next_manager_ 的指针。
static AtExitManager* g_top_manager = NULL;

AtExitManager::AtExitManager() : next_manager_(g_top_manager) {
// If multiple modules instantiate AtExitManagers they'll end up living in this
// module... they have to coexist.
#if !defined(COMPONENT_BUILD)
  DCHECK(!g_top_manager);
#endif
  g_top_manager = this;
}

AtExitManager::~AtExitManager() {
  if (!g_top_manager) {
    NOTREACHED() << "Tried to ~AtExitManager without an AtExitManager";
    return;
  }
  DCHECK_EQ(this, g_top_manager);

  // 以 LIFO(stack) 的顺序调用 RegisterCallback 注册的所有回调函数
  ProcessCallbacksNow();
  // 恢复原始 g_top_manager 指针
  g_top_manager = next_manager_;
}

// static
void AtExitManager::RegisterCallback(AtExitCallbackType func, void* param) {
  DCHECK(func);
  if (!g_top_manager) {
    NOTREACHED() << "Tried to RegisterCallback without an AtExitManager";
    return;
  }

  // 添加回调函数到栈中。({...} 以 pod struct 初始化方式产生 Callback 临时对象)
  AutoLock lock(g_top_manager->lock_);
  g_top_manager->stack_.push({func, param});
}

// static
void AtExitManager::ProcessCallbacksNow() {
  if (!g_top_manager) {
    NOTREACHED() << "Tried to ProcessCallbacksNow without an AtExitManager";
    return;
  }

  // 以 LIFO 的顺序调用 RegisterCallback 注册的所有回调函数
  AutoLock lock(g_top_manager->lock_);

  while (!g_top_manager->stack_.empty()) {
    Callback task = g_top_manager->stack_.top();
    task.func(task.param);
    g_top_manager->stack_.pop();
  }
}

AtExitManager::AtExitManager(bool shadow) : next_manager_(g_top_manager) {
  DCHECK(shadow || !g_top_manager);
  g_top_manager = this;
}

}  // namespace butil

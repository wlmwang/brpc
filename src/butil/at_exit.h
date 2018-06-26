// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 提供可控制的 atexit() 的工具：用户控制对象销毁时执行注册的回调函数

#ifndef BUTIL_AT_EXIT_H_
#define BUTIL_AT_EXIT_H_

#include <stack>

#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/synchronization/lock.h"

namespace butil {

// This class provides a facility similar to the CRT atexit(), except that
// we control when the callbacks are executed. Under Windows for a DLL they
// happen at a really bad time and under the loader lock. This facility is
// mostly used by butil::Singleton.
//
// The usage is simple. Early in the main() or WinMain() scope create an
// AtExitManager object on the stack:
// int main(...) {
//    butil::AtExitManager exit_manager;
//
// }
// When the exit_manager object goes out of scope, all the registered
// callbacks and singleton destructors will be called.
// 
// AtExitManager 提供了一个类似于 atexit() 的工具，让我们控制何时执行回调。经常
// 使用在 base::Singleton 单例里。
// 当栈上 AtExitManager exit_manager 对象超出声明域时，将调用所有当前域注册的回
// 调函数和单例的析构函数。
// 
// Use like:
// int main(...) {
//    {
//        // exit_manager out of scope,all the registered callbacks and 
//        // singleton destructors will be called.
//        butil::AtExitManager exit_manager;
//        AtExitManager::RegisterCallback(...);
//    }
// }

class BUTIL_EXPORT AtExitManager {
 public:
  // 回调函数原型
  typedef void (*AtExitCallbackType)(void*);

  AtExitManager();

  // The dtor calls all the registered callbacks. Do not try to register more
  // callbacks after this point.
  ~AtExitManager();

  // Registers the specified function to be called at exit. The prototype of
  // the callback function is void func(void*).
  // 
  // 注册在退出时调用的回调函数。回调函数的原型是 void func(void*)
  static void RegisterCallback(AtExitCallbackType func, void* param);

  // Calls the functions registered with RegisterCallback in LIFO order. It
  // is possible to register new callbacks after calling this function.
  // 
  // 以 LIFO(stack) 的顺序调用 RegisterCallback 注册的所有回调函数
  static void ProcessCallbacksNow();

 protected:
  // This constructor will allow this instance of AtExitManager to be created
  // even if one already exists.  This should only be used for testing!
  // AtExitManagers are kept on a global stack, and it will be removed during
  // destruction.  This allows you to shadow another AtExitManager.
  explicit AtExitManager(bool shadow);

 private:
  struct Callback {
    AtExitCallbackType func;
    void* param;
  };
  // g_top_manager 锁
  butil::Lock lock_;
  // 栈的方式管理注册的回调函数
  std::stack<Callback> stack_;
  // 主要用来恢复 g_top_manager 指针
  AtExitManager* next_manager_;  // Stack of managers to allow shadowing.

  DISALLOW_COPY_AND_ASSIGN(AtExitManager);
};

#if defined(UNIT_TEST)
class ShadowingAtExitManager : public AtExitManager {
 public:
  ShadowingAtExitManager() : AtExitManager(true) {}
};
#endif  // defined(UNIT_TEST)

}  // namespace butil

#endif  // BUTIL_AT_EXIT_H_

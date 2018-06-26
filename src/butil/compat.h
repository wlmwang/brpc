// Copyright (c) 2018 Baidu, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Authors: Ge,Jun (gejun@baidu.com)
//          Jiashun Zhu(zhujiashun@baidu.com)

#ifndef BUTIL_COMPAT_H
#define BUTIL_COMPAT_H

#include "butil/build_config.h"
#include <pthread.h>

// @tips
// 一. extern "C" 编译阶段影响：
// 1. 作为一种面向对象的语言，C++ 支持函数重载，而过程式语言 C 则不支持。所以，一个函数被 
// C++ 编译后在符号库中的名字与 C 语言的有所不同。例如，假设某个函数的原型为：
// void foo(int x, int y);
// 该函数被 C 编译器编译后在符号库中的名字为 _foo ，而 C++ 编译器则会产生像 _foo_int_int 
// 之类的名字（不同的编译器可能生成的名字不同，但是都采用了相同的机制，生成的新名字称为 
// "mangled name" ）。_foo_int_int 这样的名字包含了函数名、函数参数数量及类型信息，C++ 
// 就是靠这种机制来实现函数重载的。
// 2. 同样地，C++ 中的变量除支持局部变量外，还支持类成员变量和全局变量。用户所编写程序的类
// 成员变量可能与全局变量同名，我们以 "." 来区分。而本质上，编译器在进行编译时，与函数的处
// 理相似，也为类中的变量取了一个独一无二的名字，这个名字与用户程序中同名的全局变量名字不同。
// 
// 二. extern "C" 链接阶段影响：
// 假设在 C++ 中，模块 A 的头文件如下 (moduleA.h)： 
// #ifndef MODULE_A_H 
// #define MODULE_A_H 
// int foo(int x, int y);
// #endif 
// 
// 在模块 B 中引用该函数 (moduleB.cpp)：
// #include "moduleA.h" 
// foo(2,3);
// 
// 1. 如果 B 模块是 C 程序，而 A 模块是 C++ 库头文件的话，会导致链接错误（moduleA.o 将函
// 数符号编译为 _foo_int_int，B 实际链接却是 _foo 符号）
// 2. 同理，如果 B 模块是 C++ 程序，而 A 模块是 C 库的头文件也会导致错误（moduleA.o 将函
// 数符号编译为 _foo， B 实际链接却是 _foo_int_int 符号）
// 
// 三. 加 extern "C" 声明后的编译和链接方式：
// 假设在 C++ 中，模块 A 的头文件如下 (moduleA.h)： 
// #ifndef MODULE_A_H 
// #define MODULE_A_H 
// extern "C" int foo(int x, int y);
// #endif 
// 
// 在模块 B 中引用该函数 (moduleB.cpp)：
// #include "moduleA.h" 
// foo(2,3);
// 
// 1. 模块 A 编译生成 foo 的目标代码时，没有对其名字进行特殊处理，采用了 C 语言的方式。
// 2. 链接器在为模块 B 的目标代码寻找 foo(2,3) 调用时，寻找的是未经修改的符号名 _foo 符号。
// 
// 综上可知，extern "C" 这个声明的真实目的，就是实现 C++ 与 C 及其它语言的混合编程。
// 
// 
// \file <sys/cdefs.h>
// #if defined(__cplusplus) \
// #define __BEGIN_DECLS extern "C" { \
// #define __END_DECLS     }
// #else
// #define __BEGIN_DECLS
// #define __END_DECLS

#if defined(OS_MACOSX)

#include <sys/cdefs.h>
#include <stdint.h>
#include <dispatch/dispatch.h>    // dispatch_semaphore
#include <errno.h>                // EINVAL

__BEGIN_DECLS

// Implement pthread_spinlock_t for MAC.
// 
// MAC 自旋锁实现
struct pthread_spinlock_t {
    dispatch_semaphore_t sem;
};
inline int pthread_spin_init(pthread_spinlock_t *__lock, int __pshared) {
    if (__pshared != 0) {
        return EINVAL;
    }
    __lock->sem = dispatch_semaphore_create(1);
    return 0;
}
inline int pthread_spin_destroy(pthread_spinlock_t *__lock) {
    // TODO(gejun): Not see any destructive API on dispatch_semaphore
    // 
    // @todo
    // dispatch_semaphore 上没有销毁的 API
    (void)__lock;
    return 0;
}
inline int pthread_spin_lock(pthread_spinlock_t *__lock) {
    return (int)dispatch_semaphore_wait(__lock->sem, DISPATCH_TIME_FOREVER);
}
inline int pthread_spin_trylock(pthread_spinlock_t *__lock) {
    if (dispatch_semaphore_wait(__lock->sem, DISPATCH_TIME_NOW) == 0) {
        return 0;
    }
    return EBUSY;
}
inline int pthread_spin_unlock(pthread_spinlock_t *__lock) {
    return dispatch_semaphore_signal(__lock->sem);
}

__END_DECLS

#elif defined(OS_LINUX)

#include <sys/epoll.h>

#else   // defined(OS_LINUX)

#error "The platform does not support epoll-like APIs"

#endif // defined(OS_MACOSX)

__BEGIN_DECLS

// 获取线程 tid 整型值
inline uint64_t pthread_numeric_id() {
#if defined(OS_MACOSX)
    uint64_t id;
    if (pthread_threadid_np(pthread_self(), &id) == 0) {
        return id;
    }
    return -1;
#else
    return pthread_self();
#endif
}

__END_DECLS

#endif // BUTIL_COMPAT_H

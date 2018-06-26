// Copyright (c) 2016 Baidu, Inc.
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
 
// Author: Ge,Jun (gejun@baidu.com)
// Date: Thu Dec 15 14:37:39 CST 2016

#ifndef BUTIL_MEMORY_SINGLETON_ON_PTHREAD_ONCE_H
#define BUTIL_MEMORY_SINGLETON_ON_PTHREAD_ONCE_H

#include <pthread.h>
#include "butil/atomicops.h"

namespace butil {

// 一个线程安全且永不删除的全局的类型 T 的单例实现
template <typename T> class GetLeakySingleton {
public:
    static butil::subtle::AtomicWord g_leaky_singleton_untyped; // 原子单例指针
    static pthread_once_t g_create_leaky_singleton_once;
    static void create_leaky_singleton();
};
template <typename T>
butil::subtle::AtomicWord GetLeakySingleton<T>::g_leaky_singleton_untyped = 0;

template <typename T>
pthread_once_t GetLeakySingleton<T>::g_create_leaky_singleton_once = PTHREAD_ONCE_INIT;

template <typename T>
void GetLeakySingleton<T>::create_leaky_singleton() {
    T* obj = new T;
    butil::subtle::Release_Store(
        &g_leaky_singleton_untyped,
        reinterpret_cast<butil::subtle::AtomicWord>(obj));
}

// To get a never-deleted singleton of a type T, just call get_leaky_singleton<T>().
// Most daemon threads or objects that need to be always-on can be created by
// this function.
// This function can be called safely before main() w/o initialization issues of
// global variables.
// 
// 为了得到一个永不删除的全局的类型 T 的单例，只需调用 get_leaky_singleton<T>() 。大多数守护
// 线程或者需要永远存在的对象都可以通过这个函数来创建。
// 此函数可以在 main() w/o 初始化全局变量安全地调用。
template <typename T>
inline T* get_leaky_singleton() {
    const butil::subtle::AtomicWord value = butil::subtle::Acquire_Load(
        &GetLeakySingleton<T>::g_leaky_singleton_untyped);
    if (value) {
        return reinterpret_cast<T*>(value);
    }
    // 只创建一次实例
    pthread_once(&GetLeakySingleton<T>::g_create_leaky_singleton_once,
                 GetLeakySingleton<T>::create_leaky_singleton);
    return reinterpret_cast<T*>(
        GetLeakySingleton<T>::g_leaky_singleton_untyped);
}

// True(non-NULL) if the singleton is created.
// The returned object (if not NULL) can be used directly.
// 
// 全局类型 T 单实例是否已创建。若不为空，返回的实例可直接使用。
template <typename T>
inline T* has_leaky_singleton() {
    return reinterpret_cast<T*>(
        butil::subtle::Acquire_Load(
            &GetLeakySingleton<T>::g_leaky_singleton_untyped));
}

} // namespace butil

#endif // BUTIL_MEMORY_SINGLETON_ON_PTHREAD_ONCE_H

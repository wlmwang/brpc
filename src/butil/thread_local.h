// Copyright (c) 2011 Baidu, Inc.
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
// Date: Mon. Nov 7 14:47:36 CST 2011

#ifndef BUTIL_THREAD_LOCAL_H
#define BUTIL_THREAD_LOCAL_H

// @tips
// 一、线程局部存储(TLS)
// 线程局部存储提供了持久的每个线程存储空间，每个线程都拥有一份对变量的拷贝。线程局部存
// 储中的变量将一直存在，直到线程终止，届时会自动释放这一存储。要定义一个线程局部变量很
// 简单，只需在|全局或静态|变量声明中简单的包含 __thread 说明符即可。
// 
// Use like:
// static __thread int buf[MAX_ERROR_LEN];
// 
// 注意：在一个线程中可以修改另一个线程的局部变量：__thread 变量并不是在线程之间完全隐
// 藏的，每个线程都保存自己的一份拷贝，因此每个线程的这个变量的地址不同。但这个地址是整
// 个进程可见的。因此一个线程获得另外一个线程的局部变量的地址，就可以修改另一个线程的这
// 个局部变量。
// 
// 线程局部变量的声明和使用，需要注意以下几点：
// 1. 如果变量声明中使用了关键字 static 或 extern ，那么关键字 __thread 必须紧随
// 	其后。
// 2. 与一般的全局或静态变量声明一样，线程局部变量在声明时可以设置一个初始值。
// 3. 可以使用 C 语言取址操作符（&）来获取线程局部变量的地址。
// 
// C++ 中对 __thread 变量的使用有额外的限制：
// 1. 在 C++ 中，如果要在定义一个 thread-local 变量的时候做初始化，初始化的值必须是
// 	一个常量表达式。
// 2. __thread 只能修饰 POD 类型（只能用其指针类型）。
// 
// 
// 从 C++11 后添加了 thread_local 关键字。
// 
// 
// 二、线程特有数据 <pthread.h>
// __thread 是 C/C++ 编译器层实现的使用线程级局部变量的方式。而 POSIX thread 则使
// 用 getthreadspecific() 和 setthreadspecific() 组件来实现这一特性，因此编译要
// 加 -pthread 。注：使用这种方式很繁琐，并且效率低。
// 
// 使用线程特有数据需要下面几步：
// 1. 创建一个键 (key) ，用以将不同的线程特有数据区分开来。 pthread_key_create() 
// 	可创建一个 key，且只需要在首个调用该函数的线程中创建一次。
// 2. 在不同线程中，使用 pthread_setspecific() 函数将这个 key 和本线程（调用者线程）
// 	中的某个变量的值关联起来，这样就可以做到不同线程使用相同的 key 保存不同的 value 。
// 3. 在各线程可通过 pthread_getspecific() 函数来取得本线程中 key 对应的值。
// 
// Linux 支持最多 1024 个 key ，一般是 128 个，所以通常 key 是够用的。如果一个函数
// 需要多个线程特有数据的值，可以将它们封装为一个结构体，然后仅与一个 key 关联。
// 
// 
// int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
// 用于创建一个 key，成功返回 0 。函数 destructor 指向一个自定义函数，在线程终止时，
// 会自动执行该函数进行一些析构动作。例如，释放与 key 绑定的存储空间的资源，如果无需解构，
// 可将 destructor 置为 NULL（destructor 函数参数 |void* value| 是与 key 关联的
// 指向线程特有数据块的指针。注意，如果一个线程有多个"线程特有数据块"，那么对各个解构函数
// 的调用顺序是不确定的，因此每个解构函数的设计要相互独立）。
// 
// int pthread_setspecific(pthread_key_t key, const void * value);
// 用于设置 key 与本线程内某个指针或某个值的关联。成功返回 0 。
// 
// void *pthread_getspecific(pthread_key_t key);
// 用于获取 key 关联的值，由该函数的返回值的指针指向。如果 key 在该线程中尚未被关联，该
// 函数返回 NULL 。
// 
// int pthread_key_delete(pthread_key_t key);
// 用于注销一个 key ，以供下一次调用 pthread_key_create() 使用。

#include <new>                      // std::nothrow
#include <cstddef>                  // NULL
#include "butil/macros.h"            

// Provide thread_local keyword (for primitive types) before C++11
// DEPRECATED: define this keyword before C++11 might make the variable ABI
// incompatible between C++11 and C++03
// 
// 定义 C++11 之前版本 thread_local 关键字。此类型变量 C++11 和 C++03 之间可能存
// 在 ABI 不兼容。
#if !defined(thread_local) &&                                           \
    (__cplusplus < 201103L ||                                           \
     (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) < 40800)
// GCC supports thread_local keyword of C++11 since 4.8.0
#ifdef _MSC_VER
// WARNING: don't use this macro in C++03
#define thread_local __declspec(thread)
#else
// WARNING: don't use this macro in C++03
#define thread_local __thread
#endif  // _MSC_VER
#endif

#ifdef _MSC_VER
#define BAIDU_THREAD_LOCAL __declspec(thread)
#else
#define BAIDU_THREAD_LOCAL __thread
#endif  // _MSC_VER

namespace butil {

// Get a thread-local object typed T. The object will be default-constructed
// at the first call to this function, and will be deleted when thread
// exits.
// 
// 获取一个类型为 T 的线程本地静态对象指针（线程级单例）；该对象将在第一次调用此函数时默认
// 构造；并且当线程退出时被析构删除。
template <typename T> inline T* get_thread_local();

// |fn| or |fn(arg)| will be called at caller's exit. If caller is not a 
// thread, fn will be called at program termination. Calling sequence is LIFO:
// last registered function will be called first. Duplication of functions 
// are not checked. This function is often used for releasing thread-local
// resources declared with __thread (or thread_local defined in 
// butil/thread_local.h) which is much faster than pthread_getspecific or
// boost::thread_specific_ptr.
// Returns 0 on success, -1 otherwise and errno is set.
// 
// |fn| 或 |fn(arg)| 将在调用者线程退出是被调用。如果调用者不是线程，则在程序终止时调用 
// fn 。 调用序列是 LIFO : 将首先调用最后一个注册函数。不检查函数重复。此函数通常用于释放
// 使用 __thread（或 butil/thread_local.h 中定义的 thread_local ）声明的线程本地资
// 源，这比 pthread_getspecific 或 boost::thread_specific_ptr 快得多。成功时返回 
// 0 ，否则返回 -1 ，并设置 errno 。
int thread_atexit(void (*fn)());
int thread_atexit(void (*fn)(void*), void* arg);

// Remove registered function, matched functions will not be called.
// 
// 删除上面已注册的退出函数
void thread_atexit_cancel(void (*fn)());
void thread_atexit_cancel(void (*fn)(void*), void* arg);

// Delete the typed-T object whose address is `arg'. This is a common function
// to thread_atexit.
// 
// 线程本地对象退出时删除内存。
template <typename T> void delete_object(void* arg) {
    delete static_cast<T*>(arg);
}

}  // namespace butil

#include "thread_local_inl.h"

#endif  // BUTIL_THREAD_LOCAL_H

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/threading/thread_local_storage.h"

#include "butil/logging.h"

// @tips
// \file <pthread.h>
// int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
// 用于创建一个 key，成功返回 0
// 函数 destructor 指向一个自定义函数，在线程终止时，会自动执行该函数进行一些析构动作，
// 例如释放与 key 绑定的存储空间的资源，如果无需解构，可将 destructor 置为 NULL
// （ destructor 函数参数 |void* value| 是与 key 关联的指向线程特有数据块的指针。
// 注意，如果一个线程有多个线程特有数据块，那么对各个解构函数的调用顺序是不确定的，因此每个解构函数的设计要相互独立）
// 
// int pthread_setspecific(pthread_key_t key, const void * value);
// 用于设置 key 与本线程内某个指针或某个值的关联。成功返回 0
// 
// void *pthread_getspecific(pthread_key_t key);
// 用于获取 key 关联的值，由该函数的返回值的指针指向。如果 key 在该线程中尚未被关联，该函数返回 NULL
// 
// int pthread_key_delete(pthread_key_t key);
// 用于注销一个 key ，以供下一次调用 pthread_key_create() 使用

namespace butil {

namespace internal {

bool PlatformThreadLocalStorage::AllocTLS(TLSKey* key) {
  return !pthread_key_create(key,
      butil::internal::PlatformThreadLocalStorage::OnThreadExit);
}

void PlatformThreadLocalStorage::FreeTLS(TLSKey key) {
  int ret = pthread_key_delete(key);
  DCHECK_EQ(ret, 0);
}

void* PlatformThreadLocalStorage::GetTLSValue(TLSKey key) {
  return pthread_getspecific(key);
}

void PlatformThreadLocalStorage::SetTLSValue(TLSKey key, void* value) {
  int ret = pthread_setspecific(key, value);
  DCHECK_EQ(ret, 0);
}

}  // namespace internal

}  // namespace butil

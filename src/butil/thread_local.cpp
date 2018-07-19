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

#include <errno.h>                       // errno
#include <pthread.h>                     // pthread_key_t
#include <stdio.h>
#include <algorithm>                     // std::find
#include <vector>                        // std::vector
#include <stdlib.h>                      // abort, atexit

namespace butil {
namespace detail {

// 线程本地变量释放内存管理类（线程级单例）
class ThreadExitHelper {
public:
    typedef void (*Fn)(void*);
    typedef std::pair<Fn, void*> Pair;
    
    ~ThreadExitHelper() {
        // Call function reversely.
        // 
        // 反向调用注册函数（LIFO）
        while (!_fns.empty()) {
            Pair back = _fns.back();
            _fns.pop_back();
            // Notice that _fns may be changed after calling Fn.
            // 
            // 请注意，调用 Fn 后可能会更改 _fns
            back.first(back.second);
        }
    }

    // 添加退出函数
    int add(Fn fn, void* arg) {
        try {
            if (_fns.capacity() < 16) {
                _fns.reserve(16);
            }
            _fns.push_back(std::make_pair(fn, arg));
        } catch (...) {
            errno = ENOMEM;
            return -1;
        }
        return 0;
    }

    // 移除线程退出函数
    void remove(Fn fn, void* arg) {
        std::vector<Pair>::iterator
            it = std::find(_fns.begin(), _fns.end(), std::make_pair(fn, arg));
        if (it != _fns.end()) {
            std::vector<Pair>::iterator ite = it + 1;
            for (; ite != _fns.end() && ite->first == fn && ite->second == arg;
                  ++ite) {}
            _fns.erase(it, ite);
        }
    }

private:
    std::vector<Pair> _fns;
};

static pthread_key_t thread_atexit_key;
static pthread_once_t thread_atexit_once = PTHREAD_ONCE_INIT;

static void delete_thread_exit_helper(void* arg) {
    delete static_cast<ThreadExitHelper*>(arg);
}

// 在程序终止时调用注册的清理 key 关联的指向线程特有数据块的指针内存。
static void helper_exit_global() {
    detail::ThreadExitHelper* h = 
        (detail::ThreadExitHelper*)pthread_getspecific(detail::thread_atexit_key);
    if (h) {
        pthread_setspecific(detail::thread_atexit_key, NULL);
        delete h;
    }
}

// 进程级全局创建一次"线程特有数据"键 (key) 
static void make_thread_atexit_key() {
    // 进程级全局创建一次"线程特有数据"键 (key) ，并设置线程退出，清理 key 关联的指向线程特
    // 有数据块的指针内存。
    if (pthread_key_create(&thread_atexit_key, delete_thread_exit_helper) != 0) {
        fprintf(stderr, "Fail to create thread_atexit_key, abort\n");
        abort();
    }
    // If caller is not pthread, delete_thread_exit_helper will not be called.
    // We have to rely on atexit().
    // 
    // 如果调用者不是 pthread （比如主线程），则不会调用 delete_thread_exit_helper 。
    // 必须要依赖 atexit() 清理，即在程序终止时调用注册的函数。
    atexit(helper_exit_global);
}

// 获取或者创建"线程退出"管理对象（线程级单例）
detail::ThreadExitHelper* get_or_new_thread_exit_helper() {
    pthread_once(&detail::thread_atexit_once, detail::make_thread_atexit_key);

    detail::ThreadExitHelper* h =
        (detail::ThreadExitHelper*)pthread_getspecific(detail::thread_atexit_key);
    if (NULL == h) {
        // 线程特有数据块的指针尚未关联内存。创建管理对象。
        h = new (std::nothrow) detail::ThreadExitHelper;
        if (NULL != h) {
            // 关联特有数据块到 key
            pthread_setspecific(detail::thread_atexit_key, h);
        }
    }
    return h;
}

detail::ThreadExitHelper* get_thread_exit_helper() {
    pthread_once(&detail::thread_atexit_once, detail::make_thread_atexit_key);
    return (detail::ThreadExitHelper*)pthread_getspecific(detail::thread_atexit_key);
}

// 就地调用执行单参数的函数指针。与 与带参数的函数原型保持一致。
static void call_single_arg_fn(void* fn) {
    ((void (*)())fn)();
}

}  // namespace detail

int thread_atexit(void (*fn)(void*), void* arg) {
    if (NULL == fn) {
        errno = EINVAL;
        return -1;
    }
    detail::ThreadExitHelper* h = detail::get_or_new_thread_exit_helper();
    if (h) {
        // 添加退出函数指针
        return h->add(fn, arg);
    }
    errno = ENOMEM;
    return -1;
}

int thread_atexit(void (*fn)()) {
    if (NULL == fn) {
        errno = EINVAL;
        return -1;
    }
    // 为了与带参数的函数原型一致。不带参数的 fn 调用被封装在 call_single_arg_fn 中
    return thread_atexit(detail::call_single_arg_fn, (void*)fn);
}

void thread_atexit_cancel(void (*fn)(void*), void* arg) {
    if (fn != NULL) {
        detail::ThreadExitHelper* h = detail::get_thread_exit_helper();
        if (h) {
            h->remove(fn, arg);
        }
    }
}

void thread_atexit_cancel(void (*fn)()) {
    if (NULL != fn) {
        // 为了与带参数的函数原型一致。不带参数的 fn 调用被封装在 call_single_arg_fn 中
        thread_atexit_cancel(detail::call_single_arg_fn, (void*)fn);
    }
}

}  // namespace butil

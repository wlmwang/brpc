#ifndef BUTIL_INTRUSIVE_PTR_HPP
#define BUTIL_INTRUSIVE_PTR_HPP

//  Copyright (c) 2001, 2002 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org/libs/smart_ptr/intrusive_ptr.html for documentation.
//
//  intrusive_ptr
//
//  A smart pointer that uses intrusive reference counting.
//
//  Relies on unqualified calls to
//  
//      void intrusive_ptr_add_ref(T * p);
//      void intrusive_ptr_release(T * p);
//
//          (p != 0)
//
//  The object is responsible for destroying itself.
//  
//  
// 一个“侵入式”引用计数器智能指针，它实际并不提供引用计数功能，而是要求被存储的对象自己实
// 现引用计数功能。并提供 intrusive_ptr_add_ref 和 intrusive_ptr_release 函数接口
// 供 intrusive_ptr 调用。
// 
// 
// @tips
// std::shared_ptr 的一大陷阱就是用一个 raw pointer 多次创建 shared_ptr ，将导致该 
// raw pointer 在 shared_ptr 析构时被多次销毁。即不能如下使用：
// int *a = new int(5);
// std::shared_ptr ptr1(a);
// std::shared_ptr ptr2(a);　　// 错误
// 
// （以下的 intrusive_ptr_base 是一个带引用计数器属性的模板基类(mutable boost::detail
// ::atomic_count ref_count)，intrusive_ptr_add_ref() 和 intrusive_ptr_release() 
// 函数来提供引用计数功能）。
// 
// intrusive_ptr 完全具备 std::shared_ptr 的功能，且不存在 std::shared_ptr 的问题。
// 即，可以利用 raw pointer 创建多个 intrusive _ptr 。其原因就在于引用计数的 ref_count 
// 属性：在 shared_ptr 是放在 shared_ptr 结构里的，而 intrusive_ptr 目标对象 T 需通过
// 继承 intrusive_ptr_base 将引用计数作为 T 对象的内部成员变量，就不会出现同一个对象有两
// 个引用计数器的情况出现。
// 
// 那么为什么通常鼓励大家使用 std::shared_ptr ，而不是 intrusive_ptr 呢？因为 shared_ptr 
// 不是侵入性的，它可以指向任意类型的对象。而 intrusive_ptr 所要指向的对象，需要继承 
// intrusive_ptr_base ，即使不需要，引用计数成员也会被创建。
// 
// 结论：如果创建新类且需要进行传递，则继承 intrusive_ptr_base ，使用 intrusive_ptr

#include <functional>
#include <cstddef>
#include <ostream>
#include "butil/build_config.h"
#include "butil/containers/hash_tables.h"

namespace butil {

namespace detail {

// NOTE: sp_convertible is different from butil::is_convertible
// (in butil/type_traits.h) that it converts pointers only. Using
// butil::is_convertible results in ctor/dtor issues.
// 
// 注意： sp_convertible 与 butil::is_convertible(butil/type_traits.h) 不
// 同，它只转换指针。使用 butil::is_convertible 会导致 ctor/dtor 问题。
template< class Y, class T > struct sp_convertible {
    typedef char (&yes) [1];
    typedef char (&no)  [2];

    static yes f( T* );
    static no  f( ... );

    // 比较 Y* 与 T* 是否是相同类型（包括 Y 是否是 T 派生类），是为 true
    enum _vt { value = sizeof((f)(static_cast<Y*>(0))) == sizeof(yes) };
};
// "T 基类" 不能是数组类型。
template< class Y, class T > struct sp_convertible<Y, T[]> {
    enum _vt { value = false };
};
// Y/T 都是数组时，判断元素之间是否是相同类型（包括 Y 是否是 T 派生类）
template< class Y, class T > struct sp_convertible<Y[], T[]> {
    enum _vt { value = sp_convertible<Y[1], T[1]>::value };
};
template<class Y, std::size_t N, class T> struct sp_convertible<Y[N], T[]> {
    enum _vt { value = sp_convertible<Y[1], T[1]>::value };
};

struct sp_empty {};
template< bool > struct sp_enable_if_convertible_impl;
template<> struct sp_enable_if_convertible_impl<true> { typedef sp_empty type; };
template<> struct sp_enable_if_convertible_impl<false> {};
template< class Y, class T > struct sp_enable_if_convertible
    : public sp_enable_if_convertible_impl<sp_convertible<Y, T>::value> {};

} // namespace detail

// 一个“侵入式”引用计数器智能指针。它实际并不提供引用计数功能，而是要求被存储的对象自己实现
// 引用计数功能。
// 
// 构造 && 拷贝构造时增加引用计数器。析构时，减少引用计数器。赋值 && 移动拷贝构造时，引用
// 计数器不变。
template<class T> class intrusive_ptr {
private:
    typedef intrusive_ptr this_type;
public:
    typedef T element_type;
    intrusive_ptr() BAIDU_NOEXCEPT : px(0) {}

    intrusive_ptr(T * p, bool add_ref = true): px(p) {
        // 默认增加引用计数器
        if(px != 0 && add_ref) intrusive_ptr_add_ref(px);
    }

    // intrusive_ptr<> 任意类型的拷贝初始化。
    // 当 U/T 是两个不同（或不是子父级）类型时，编译出错（可能特化类中 type 没有被定义）。
    template<class U>
    intrusive_ptr(const intrusive_ptr<U>& rhs,
                  typename detail::sp_enable_if_convertible<U,T>::type = detail::sp_empty())
        : px(rhs.get()) {
        if(px != 0) intrusive_ptr_add_ref(px);
    }

    // intrusive_ptr 同类型拷贝初始化
    intrusive_ptr(const intrusive_ptr& rhs): px(rhs.px) {
        if(px != 0) intrusive_ptr_add_ref(px);
    }

    // 减少引用计数器
    ~intrusive_ptr() {
        if(px != 0) intrusive_ptr_release(px);
    }

    template<class U> intrusive_ptr & operator=(const intrusive_ptr<U>& rhs) {
        // 注：仅交换（与 rhs 副本），不更新引用计数器
        this_type(rhs).swap(*this);
        return *this;
    }

// Move support
// 
// 移动拷贝成员
#if defined(BUTIL_CXX11_ENABLED)
    intrusive_ptr(intrusive_ptr && rhs) BAIDU_NOEXCEPT : px(rhs.px) {
        rhs.px = 0;
    }

    intrusive_ptr & operator=(intrusive_ptr && rhs) BAIDU_NOEXCEPT {
        // 注：仅交换（与 rhs 移动构造的副本），不更新引用计数器
        this_type(static_cast< intrusive_ptr && >(rhs)).swap(*this);
        return *this;
    }
#endif

    intrusive_ptr & operator=(const intrusive_ptr& rhs) {
        // 注：仅交换（与 rhs 副本），不更新引用计数器
        this_type(rhs).swap(*this);
        return *this;
    }

    intrusive_ptr & operator=(T * rhs) {
        this_type(rhs).swap(*this);
        return *this;
    }

    void reset() BAIDU_NOEXCEPT {
        // 注：仅交换（与 intrusive_ptr 副本），不更新引用计数器
        // 释放当前所有权
        this_type().swap(*this);
    }

    void reset(T * rhs) {
        this_type(rhs).swap(*this);
    }

    void reset(T * rhs, bool add_ref) {
        this_type(rhs, add_ref).swap(*this);
    }

    // 获取原始指针
    T * get() const BAIDU_NOEXCEPT {
        return px;
    }

    // 解除与 px 绑定关系
    T * detach() BAIDU_NOEXCEPT {
        T * ret = px;
        px = 0;
        return ret;
    }

    T & operator*() const {
        return *px;
    }

    T * operator->() const {
        return px;
    }

    // implicit conversion to "bool"
#if defined(BUTIL_CXX11_ENABLED)
    explicit operator bool () const BAIDU_NOEXCEPT {
        return px != 0;
    }
#elif defined(__CINT__)
    operator bool () const BAIDU_NOEXCEPT {
        return px != 0;
    }
#elif (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ < 304))
    typedef element_type * (this_type::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const BAIDU_NOEXCEPT {
        return px == 0? 0: &this_type::get;
    }
#else
    typedef element_type * this_type::*unspecified_bool_type;
    operator unspecified_bool_type() const BAIDU_NOEXCEPT {
        return px == 0? 0: &this_type::px;
    }
#endif

    // operator! is redundant, but some compilers need it
    bool operator! () const BAIDU_NOEXCEPT {
        return px == 0;
    }

    // intrusive_ptr 交换
    void swap(intrusive_ptr & rhs) BAIDU_NOEXCEPT {
        T * tmp = px;
        px = rhs.px;
        rhs.px = tmp;
    }

private:
    T * px;
};

// intrusive_ptr 间以及与原始指针间关系运算符重载
template<class T, class U>
inline bool operator==(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) {
    return a.get() == b.get();
}

template<class T, class U>
inline bool operator!=(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) {
    return a.get() != b.get();
}

template<class T, class U>
inline bool operator==(const intrusive_ptr<T>& a, U * b) {
    return a.get() == b;
}

template<class T, class U>
inline bool operator!=(const intrusive_ptr<T>& a, U * b) {
    return a.get() != b;
}

template<class T, class U>
inline bool operator==(T * a, const intrusive_ptr<U>& b) {
    return a == b.get();
}

template<class T, class U>
inline bool operator!=(T * a, const intrusive_ptr<U>& b) {
    return a != b.get();
}

#if __GNUC__ == 2 && __GNUC_MINOR__ <= 96
// Resolve the ambiguity between our op!= and the one in rel_ops
template<class T>
inline bool operator!=(const intrusive_ptr<T>& a, const intrusive_ptr<T>& b) {
    return a.get() != b.get();
}
#endif

// intrusive_ptr 与 C++11 中预定义 nullptr 字面量（关键字）之间比较运算符。
#if defined(BUTIL_CXX11_ENABLED)
template<class T>
inline bool operator==(const intrusive_ptr<T>& p, std::nullptr_t) BAIDU_NOEXCEPT {
    return p.get() == 0;
}
template<class T>
inline bool operator==(std::nullptr_t, const intrusive_ptr<T>& p) BAIDU_NOEXCEPT {
    return p.get() == 0;
}

template<class T>
inline bool operator!=(const intrusive_ptr<T>& p, std::nullptr_t) BAIDU_NOEXCEPT {
    return p.get() != 0;
}
template<class T>
inline bool operator!=(std::nullptr_t, const intrusive_ptr<T>& p) BAIDU_NOEXCEPT {
    return p.get() != 0;
}
#endif  // BUTIL_CXX11_ENABLED

// 小于比较运算符（标准容器多有使用）
template<class T>
inline bool operator<(const intrusive_ptr<T>& a, const intrusive_ptr<T>& b) {
    return std::less<T *>()(a.get(), b.get());
}

template<class T> void swap(intrusive_ptr<T> & lhs, intrusive_ptr<T> & rhs) {
    lhs.swap(rhs);
}

// mem_fn support

// 获取原始指针
template<class T> T * get_pointer(const intrusive_ptr<T>& p) {
    return p.get();
}

// "类型转换"
template<class T, class U> intrusive_ptr<T> static_pointer_cast(const intrusive_ptr<U>& p) {
    return static_cast<T *>(p.get());
}

template<class T, class U> intrusive_ptr<T> const_pointer_cast(const intrusive_ptr<U>& p) {
    return const_cast<T *>(p.get());
}

template<class T, class U> intrusive_ptr<T> dynamic_pointer_cast(const intrusive_ptr<U>& p) {
    return dynamic_cast<T *>(p.get());
}

template<class Y> std::ostream & operator<< (std::ostream & os, const intrusive_ptr<Y>& p) {
    os << p.get();
    return os;
}

} // namespace butil

// hash_value
// 
// 便于让 intrusive_ptr 用作 unordered_map 等容器中的键
namespace BUTIL_HASH_NAMESPACE {

#if defined(COMPILER_GCC)
template<typename T>
struct hash<butil::intrusive_ptr<T> > {
    std::size_t operator()(const butil::intrusive_ptr<T>& p) const {
        return hash<T*>()(p.get());
    }
};
#elif defined(COMPILER_MSVC)
template<typename T>
inline size_t hash_value(const butil::intrusive_ptr<T>& sp) {
    return hash_value(p.get());
}
#endif  // COMPILER

}  // namespace BUTIL_HASH_NAMESPACE

#endif  // BUTIL_INTRUSIVE_PTR_HPP

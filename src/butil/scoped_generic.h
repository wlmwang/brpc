// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 这个类的作用类似于 scoped_ptr (自带自定义的删除器)。但相比于 scoped_ptr 管理
// 指针， ScopedGeneric 管理的是对象的副本（非指针资源）。
// 场景如： ScopedFD 就是 ScopedGeneric 的一个特化版本，用来管理文件描述符。

#ifndef BUTIL_SCOPED_GENERIC_H_
#define BUTIL_SCOPED_GENERIC_H_

#include <stdlib.h>

#include <algorithm>

#include "butil/compiler_specific.h"
#include "butil/move.h"

namespace butil {

// This class acts like ScopedPtr with a custom deleter (although is slightly
// less fancy in some of the more escoteric respects) except that it keeps a
// copy of the object rather than a pointer, and we require that the contained
// object has some kind of "invalid" value.
//
// Defining a scoper based on this class allows you to get a scoper for
// non-pointer types without having to write custom code for set, reset, and
// move, etc. and get almost identical semantics that people are used to from
// scoped_ptr.
//
// It is intended that you will typedef this class with an appropriate deleter
// to implement clean up tasks for objects that act like pointers from a
// resource management standpoint but aren't, such as file descriptors and
// various types of operating system handles. Using scoped_ptr for these
// things requires that you keep a pointer to the handle valid for the lifetime
// of the scoper (which is easy to mess up).
//
// For an object to be able to be put into a ScopedGeneric, it must support
// standard copyable semantics and have a specific "invalid" value. The traits
// must define a free function and also the invalid value to assign for
// default-constructed and released objects.
//
//   struct FooScopedTraits {
//     // It's assumed that this is a fast inline function with little-to-no
//     // penalty for duplicate calls. This must be a static function even
//     // for stateful traits.
//     static int InvalidValue() {
//       return 0;
//     }
//
//     // This free function will not be called if f == InvalidValue()!
//     static void Free(int f) {
//       ::FreeFoo(f);
//     }
//   };
//
//   typedef ScopedGeneric<int, FooScopedTraits> ScopedFoo;
//
//    
// 这个类的作用类似于 scoped_ptr(自带自定义的删除器)（虽然在一些更隐蔽的方面略微少了），
// 除了它保存对象的副本而不是一个指针，所以我们要求所包含的对象有某种 "无效" 值。
//
// 基于此类定义一个 scoper 可以让你获得非指针类型的 scoper ，而无需编写自定义代码进行 
// set ，reset 和 move 等操作，并获得与 scoped_ptr 中使用的几乎相同的语义。
//
// 对于能够放入 ScopedGeneric 的对象，它必须支持标准的可复制语义并具有特定的 "无效" 
// 值。特征 traits 必须定义一个 Free 函数，并为默认构造和释放的对象分配 "无效" 值。
// 
// Use like:
// 
//   struct FooScopedTraits {
//     // 假设这是一个快速的内联函数，重复调用几乎没有惩罚
//     // 即使对于状态 traits，这也是一个静态函数。
//     static int InvalidValue() {
//       return 0;
//     }
//
//     // 当 f = InvalidValue() ，Free 不会被调用（不重复销毁资源）
//     static void Free(int f) {
//       ::FreeFoo(f);
//     }
//   };
//   typedef ScopedGeneric<int, FooScopedTraits> ScopedFoo;
//
template<typename T, typename Traits>
class ScopedGeneric {
  MOVE_ONLY_TYPE_FOR_CPP_03(ScopedGeneric, RValue)

 private:
  // This must be first since it's used inline below.
  //
  // Use the empty base class optimization to allow us to have a D
  // member, while avoiding any space overhead for it when D is an
  // empty class.  See e.g. http://www.cantrip.org/emptyopt.html for a good
  // discussion of this technique.
  // 
  // 使用空的基类优化来允许我们拥有一个 D 成员，同时在 D 是一个空类时避免空间开销。
  struct Data : public Traits {
    explicit Data(const T& in) : generic(in) {}
    Data(const T& in, const Traits& other) : Traits(other), generic(in) {}
    T generic;  // 资源副本
  };

 public:
  typedef T element_type;     // 元素类型
  typedef Traits traits_type; // traits 类型

  ScopedGeneric() : data_(traits_type::InvalidValue()) {}

  // Constructor. Takes responsibility for freeing the resource associated with
  // the object T.
  explicit ScopedGeneric(const element_type& value) : data_(value) {}

  // Constructor. Allows initialization of a stateful traits object.
  ScopedGeneric(const element_type& value, const traits_type& traits)
      : data_(value, traits) {
  }

  // Move constructor for C++03 move emulation.
  // 
  // C++03 移动构造函数
  ScopedGeneric(RValue rvalue)
      : data_(rvalue.object->release(), rvalue.object->get_traits()) {
  }

  ~ScopedGeneric() {
    FreeIfNecessary();
  }

  // Frees the currently owned object, if any. Then takes ownership of a new
  // object, if given. Self-resets are not allowd as on scoped_ptr. See
  // http://crbug.com/162971
  // 
  // 释放当前拥有的对象（如果有的话）。然后获得一个新对象的所有权（如果给定）。
  void reset(const element_type& value = traits_type::InvalidValue()) {
    if (data_.generic != traits_type::InvalidValue() && data_.generic == value)
      abort();
    FreeIfNecessary();
    data_.generic = value;
  }

  void swap(ScopedGeneric& other) {
    // Standard swap idiom: 'using std::swap' ensures that std::swap is
    // present in the overload set, but we call swap unqualified so that
    // any more-specific overloads can be used, if available.
    using std::swap;
    swap(static_cast<Traits&>(data_), static_cast<Traits&>(other.data_));
    swap(data_.generic, other.data_.generic);
  }

  // Release the object. The return value is the current object held by this
  // object. After this operation, this object will hold a null value, and
  // will not own the object any more.
  // 
  // 释放当前持有指针所有权，作为返回值。以便实现移动语义。
  element_type release() WARN_UNUSED_RESULT {
    element_type old_generic = data_.generic;
    data_.generic = traits_type::InvalidValue();
    return old_generic;
  }

  // 返回原始资源
  const element_type& get() const { return data_.generic; }

  // Returns true if this object doesn't hold the special null value for the
  // associated data type.
  bool is_valid() const { return data_.generic != traits_type::InvalidValue(); }

  bool operator==(const element_type& value) const {
    return data_.generic == value;
  }
  bool operator!=(const element_type& value) const {
    return data_.generic != value;
  }

  // 返回原始 traits
  Traits& get_traits() { return data_; }
  const Traits& get_traits() const { return data_; }

 private:
  void FreeIfNecessary() {
    if (data_.generic != traits_type::InvalidValue()) {
      data_.Free(data_.generic);
      data_.generic = traits_type::InvalidValue();
    }
  }

  // Forbid comparison. If U != T, it totally doesn't make sense, and if U ==
  // T, it still doesn't make sense because you should never have the same
  // object owned by two different ScopedGenerics.
  // 
  // 禁止不同类型的比较操作，即，不允许 U != T  或 U == T 表达式。
  template <typename T2, typename Traits2> bool operator==(
      const ScopedGeneric<T2, Traits2>& p2) const;
  template <typename T2, typename Traits2> bool operator!=(
      const ScopedGeneric<T2, Traits2>& p2) const;

  Data data_;
};

template<class T, class Traits>
void swap(const ScopedGeneric<T, Traits>& a,
          const ScopedGeneric<T, Traits>& b) {
  a.swap(b);
}

template<class T, class Traits>
bool operator==(const T& value, const ScopedGeneric<T, Traits>& scoped) {
  return value == scoped.get();
}

template<class T, class Traits>
bool operator!=(const T& value, const ScopedGeneric<T, Traits>& scoped) {
  return value != scoped.get();
}

}  // namespace butil

#endif  // BUTIL_SCOPED_GENERIC_H_

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

// Get name of a class. For example, class_name<T>() returns the name of T
// (with namespace prefixes). This is useful in template classes.
// 
// 获取一个类的名字。例如，class_name<T>() 返回 T 的名称（带有命名空间前缀）。多用
// 于模版类中。

#ifndef BUTIL_CLASS_NAME_H
#define BUTIL_CLASS_NAME_H

#include <typeinfo>
#include <string>                                // std::string

namespace butil {

// 转换 |name| 为人类可读类名字符串。 |name| = typeid(T).name() 为 C++ ABI 规
// 则下的有效名称。
std::string demangle(const char* name);

namespace detail {
template <typename T> struct ClassNameHelper { static std::string name; };
// 静态成员初始化
template <typename T> std::string ClassNameHelper<T>::name = demangle(typeid(T).name());
}

// Get name of class |T|, in std::string.
// 
// 返回类 |T| 的类名字符串(std::string)。指向 ClassNameHelper<T> 模版类的静态属性 |name| 的引用。
template <typename T> const std::string& class_name_str() {
    // We don't use static-variable-inside-function because before C++11
    // local static variable is not guaranteed to be thread-safe.
    return detail::ClassNameHelper<T>::name;
}

// Get name of class |T|, in const char*.
// Address of returned name never changes.
// 
// 返回类 |T| 的类名字符串(const char*)。指向 ClassNameHelper<T> 模版类的静态属性 |name| 的指针。
template <typename T> const char* class_name() {
    return class_name_str<T>().c_str();
}

// Get typename of |obj|, in std::string
// 
// 返回类对象 |obj| 的类名字符串(std::string)。
template <typename T> std::string class_name_str(T const& obj) {
    return demangle(typeid(obj).name());
}

}  // namespace butil

#endif  // BUTIL_CLASS_NAME_H

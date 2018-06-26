// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @tips
// 在一个包含很多文件的共享库(.so)中，如果希望某个符号可以被共享库内部的几个文件访问，而又
// 不提供给外部使用，则对符号进行隐藏处理就会比较困难。大多数的连接器都提供一些便利的方法来
// 隐藏或者显示模块中所有的符号。但如果希望更加具有选择性，则需要更多的处理。
// 
// 符号可见性属性可以应用到：函数，变量，模板，以及 C++ 类。
// 
// 如果一个类被标识为 hidden ，则该类的所有成员函数，静态成员变量，以及编译器生成的元数据，
// 比如虚函数表和 RTTI 信息也都会被隐藏。
// 
// GCC 4.0 支持一个选项，用于设置源文件中符号的缺省可见性。这个选项是 -fvisibility=vis,
// 该值可以是 default(对外可见)/hidden(隐藏)。你可以用它来设置当前编译的符号的可见性。
// 1. 设置为 default 时，没有显式标识为 hidden 的符号都处理为可见。
// 2. 设置为 hidden 时，没有显式标识为 default 可见的符号都处理为隐藏。
// 3. 如果您在编译中没有指定 -fvisibility 选项，编译器会自行处理为 default 缺省的可见性。
// 注意：default 设置不是指编译器缺省的处理方式。和 hidden 设置一样，default 来自 ELF 
// 格式定义的可见性名称。具有 default 可见性的符号和所有不使用特殊机制的符号具有相同的可见
// 性类型; 也就是说，它将作为公共接口的一部分输出(对外可见)。
// 
// 可见性属性会覆盖编译时通过 -fvisibility 选项指定的值。
// 因此，增加 default 可见性属性会使符号在所有情况下都被输出，反过来，增加 hidden 可见性
// 属性会隐藏相应的符号。
// Use like:
// __attribute__((visibility("default"))) void MyFunction1() {}
// __attribute__((visibility("hidden"))) void MyFunction2() {}

#ifndef BUTIL_BASE_EXPORT_H_
#define BUTIL_BASE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BUTIL_IMPLEMENTATION)
#define BUTIL_EXPORT __declspec(dllexport)
#define BUTIL_EXPORT_PRIVATE __declspec(dllexport)
#else
#define BUTIL_EXPORT __declspec(dllimport)
#define BUTIL_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(BUTIL_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(BUTIL_IMPLEMENTATION)
#define BUTIL_EXPORT __attribute__((visibility("default")))
#define BUTIL_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define BUTIL_EXPORT
#define BUTIL_EXPORT_PRIVATE
#endif  // defined(BUTIL_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define BUTIL_EXPORT
#define BUTIL_EXPORT_PRIVATE
#endif

#endif  // BUTIL_BASE_EXPORT_H_

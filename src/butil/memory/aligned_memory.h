// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AlignedMemory is a POD type that gives you a portable way to specify static
// or local stack data of a given alignment and size. For example, if you need
// static storage for a class, but you want manual control over when the object
// is constructed and destructed (you don't want static initialization and
// destruction), use AlignedMemory:
//
//   static AlignedMemory<sizeof(MyClass), ALIGNOF(MyClass)> my_class;
//
//   // ... at runtime:
//   new(my_class.void_data()) MyClass();
//
//   // ... use it:
//   MyClass* mc = my_class.data_as<MyClass>();
//
//   // ... later, to destruct my_class:
//   my_class.data_as<MyClass>()->MyClass::~MyClass();
//
// Alternatively, a runtime sized aligned allocation can be created:
//
//   float* my_array = static_cast<float*>(AlignedAlloc(size, alignment));
//
//   // ... later, to release the memory:
//   AlignedFree(my_array);
//
// Or using scoped_ptr:
//
//   scoped_ptr<float, AlignedFreeDeleter> my_array(
//       static_cast<float*>(AlignedAlloc(size, alignment)));
// 
// 
// AlignedMemory 是一种 POD 类型，它提供了一种可移植的方式，来声明指定对齐大小的
// 静态或本地栈数据。例如，如果您需要一个静态存储的类，但是您希望手动控制对象的构造
// 和析构（不想静态初始化和析构），请使用 AlignedMemory. (重载 AlignedMemory 
// 让 operator new/operator delete 都声明为私用方法)。
// 
// Use like:
//   static AlignedMemory<sizeof(MyClass), ALIGNOF(MyClass)> my_class;
//
//   // 构造: placement new 方式，触发调用 MyClass 构造函数
//   new(my_class.void_data()) MyClass();
//
//   // 使用: 底层 static_cast<MyClass*>(void_data())
//   MyClass* mc = my_class.data_as<MyClass>();
//
//   // 析构: 手动调用 ~MyClass 析构函数
//   my_class.data_as<MyClass>()->MyClass::~MyClass();
//
//
// 或者，可以运行时创建指定大小的对齐分配：
//
//   float* my_array = static_cast<float*>(AlignedAlloc(size, alignment));
//
//   // 释放
//   AlignedFree(my_array);
//
// 或者，使用 scoped_ptr:
//
//   scoped_ptr<float, AlignedFreeDeleter> my_array(
//       static_cast<float*>(AlignedAlloc(size, alignment)));
// 

#ifndef BUTIL_MEMORY_ALIGNED_MEMORY_H_
#define BUTIL_MEMORY_ALIGNED_MEMORY_H_

#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/compiler_specific.h"

#if defined(COMPILER_MSVC)
#include <malloc.h>
#else
#include <stdlib.h>
#endif

namespace butil {

// AlignedMemory is specialized for all supported alignments.
// Make sure we get a compiler error if someone uses an unsupported alignment.
// 
// 如果有人使用不支持对齐的方式，确保得到一个编译器错误。 
// |Size| 类型大小。 |ByteAlignment| 对齐字节数。
template <size_t Size, size_t ByteAlignment>
struct AlignedMemory {};

// AlignedMemory 的偏特化宏。关键点：数据成员 data_[Size] 以对齐声明。
#define BUTIL_DECL_ALIGNED_MEMORY(byte_alignment) \
    template <size_t Size> \
    class AlignedMemory<Size, byte_alignment> { \
     public: \
      ALIGNAS(byte_alignment) uint8_t data_[Size]; \
      void* void_data() { return static_cast<void*>(data_); } \
      const void* void_data() const { \
        return static_cast<const void*>(data_); \
      } \
      template<typename Type> \
      Type* data_as() { return static_cast<Type*>(void_data()); } \
      template<typename Type> \
      const Type* data_as() const { \
        return static_cast<const Type*>(void_data()); \
      } \
     private: \
      void* operator new(size_t); \
      void operator delete(void*); \
    }

// Specialization for all alignments is required because MSVC (as of VS 2008)
// does not understand ALIGNAS(ALIGNOF(Type)) or ALIGNAS(template_param).
// Greater than 4096 alignment is not supported by some compilers, so 4096 is
// the maximum specified here.
// 
// 因为 MSVC (VS 2008) 不理解 ALIGNAS(ALIGNOF(Type)) or ALIGNAS(template_param) ，
// 所以声明所有对齐大小。某些编译器不支持大于 4096 对齐，因此 4096 是此处指定的最大值。
BUTIL_DECL_ALIGNED_MEMORY(1);
BUTIL_DECL_ALIGNED_MEMORY(2);
BUTIL_DECL_ALIGNED_MEMORY(4);
BUTIL_DECL_ALIGNED_MEMORY(8);
BUTIL_DECL_ALIGNED_MEMORY(16);
BUTIL_DECL_ALIGNED_MEMORY(32);
BUTIL_DECL_ALIGNED_MEMORY(64);
BUTIL_DECL_ALIGNED_MEMORY(128);
BUTIL_DECL_ALIGNED_MEMORY(256);
BUTIL_DECL_ALIGNED_MEMORY(512);
BUTIL_DECL_ALIGNED_MEMORY(1024);
BUTIL_DECL_ALIGNED_MEMORY(2048);
BUTIL_DECL_ALIGNED_MEMORY(4096);

#undef BUTIL_DECL_ALIGNED_MEMORY

// 对齐方式申请内存资源
BUTIL_EXPORT void* AlignedAlloc(size_t size, size_t alignment);

// 对齐方式释放内存资源
inline void AlignedFree(void* ptr) {
#if defined(COMPILER_MSVC)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

// Deleter for use with scoped_ptr. E.g., use as
//   scoped_ptr<Foo, butil::AlignedFreeDeleter> foo;
//   
// scoped_ptr 的删除器.
// 
// Use like:
// scoped_ptr<float, AlignedFreeDeleter> my_array(
//     static_cast<float*>(AlignedAlloc(size, alignment)));
struct AlignedFreeDeleter {
  inline void operator()(void* ptr) const {
    AlignedFree(ptr);
  }
};

}  // namespace butil

#endif  // BUTIL_MEMORY_ALIGNED_MEMORY_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_ENVIRONMENT_H_
#define BUTIL_ENVIRONMENT_H_

#include <map>
#include <string>

#include "butil/base_export.h"
#include "butil/memory/scoped_ptr.h"
#include "butil/strings/string16.h"
#include "butil/build_config.h"

namespace butil {

namespace env_vars {

#if defined(OS_POSIX)
// 在 Posix 系统上，此变量包含用户主目录的位置 (e.g, /home/username/)
BUTIL_EXPORT extern const char kHome[];
#endif

}  // namespace env_vars

// 环境变量操作接口类（虚基类）
class BUTIL_EXPORT Environment {
 public:
  virtual ~Environment();

  // Static factory method that returns the implementation that provide the
  // appropriate platform-specific instance.
  // 
  // 静态工厂方法，返回提供适当的特定于平台的实现。
  static Environment* Create();

  // Gets an environment variable's value and stores it in |result|.
  // Returns false if the key is unset.
  // 
  // 获取环境变量的值并存储在 |result| 中。如果键未设置，则返回 false
  virtual bool GetVar(const char* variable_name, std::string* result) = 0;

  // Syntactic sugar for GetVar(variable_name, NULL);
  // 
  // GetVar 的语法糖(variable_name, NULL)
  virtual bool HasVar(const char* variable_name);

  // Returns true on success, otherwise returns false.
  // 
  // 设置环境变量
  virtual bool SetVar(const char* variable_name,
                      const std::string& new_value) = 0;

  // Returns true on success, otherwise returns false.
  // 
  // 销毁环境变量
  virtual bool UnSetVar(const char* variable_name) = 0;
};


#if defined(OS_WIN)

typedef string16 NativeEnvironmentString;
typedef std::map<NativeEnvironmentString, NativeEnvironmentString>
    EnvironmentMap;

// Returns a modified environment vector constructed from the given environment
// and the list of changes given in |changes|. Each key in the environment is
// matched against the first element of the pairs. In the event of a match, the
// value is replaced by the second of the pair, unless the second is empty, in
// which case the key-value is removed.
//
// This Windows version takes and returns a Windows-style environment block
// which is a concatenated list of null-terminated 16-bit strings. The end is
// marked by a double-null terminator. The size of the returned string will
// include the terminators.
// 
// 返回从给定环境字符串(|env|)和给定更改环境列表(|changes|)，合并两者得到最终的环境变
// 量字符串。合并算法为： |env| 环境变量字符串中的每个键与 |changes| 的键在匹配的情况
// 下，值将替换为 |changes| 中的值（为空时代表删除）。不匹配情况下，取两个并集。
// 
// 这个 Windows 版本接收并返回一个 Windows 风格的环境块，它是一个以 null 结尾的 16 位
// 字符串的列表。结尾用双空终止符标记。返回的字符串的大小将包括终结符。
BUTIL_EXPORT string16 AlterEnvironment(const wchar_t* env,
                                      const EnvironmentMap& changes);

#elif defined(OS_POSIX)

typedef std::string NativeEnvironmentString;
typedef std::map<NativeEnvironmentString, NativeEnvironmentString>
    EnvironmentMap;

// See general comments for the Windows version above.
//
// This Posix version takes and returns a Posix-style environment block, which
// is a null-terminated list of pointers to null-terminated strings. The
// returned array will have appended to it the storage for the array itself so
// there is only one pointer to manage, but this means that you can't copy the
// array without keeping the original around.
// 
// 从给定环境字符串(|env|)和给定更改环境列表(|changes|)，合并两者得到最终的环境变量字符串。
// 合并算法为： |env| 环境变量字符串中的每个键与 |changes| 的键在匹配的情况下，值将替换为 
// |changes| 中的值（为空时代表删除）。不匹配情况下，取两个并集。
// 
// 函数返回的指针数组：一个足够存储所有环境块键值对(以 null 结尾字符串) + 每个环境块键值对
// 开始地址(以 null 结尾)的内存。获取时：
// 
// Use like:
// scoped_ptr<char*[]> result = AlterEnvironment(env, EnvironmentMap());
// for (size_t** indices = result.get(); !*indices; indices++) {
//      char* kv = reinterpret_cast<char*>(*indices);
//      std::cout << "env:" << kv << std::endl;
// }
BUTIL_EXPORT scoped_ptr<char*[]> AlterEnvironment(
    const char* const* env,
    const EnvironmentMap& changes);

#endif

}  // namespace butil

#endif  // BUTIL_ENVIRONMENT_H_

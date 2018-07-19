// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Location 提供了一个被构建对象（函数名/文件名/行号/上级调用地址）的基本信息存储/获
// 取途径。

#ifndef BUTIL_LOCATION_H_
#define BUTIL_LOCATION_H_

#include <string>

#include "butil/base_export.h"
#include "butil/basictypes.h"

namespace tracked_objects {

// Location provides basic info where of an object was constructed, or was
// significantly brought to life.
// 
// Location 提供了一个被构建对象（函数名/文件名/行号/上级调用地址）的基本信息存储/获
// 取途径。
class BUTIL_EXPORT Location {
 public:
  // Constructor should be called with a long-lived char*, such as __FILE__.
  // It assumes the provided value will persist as a global constant, and it
  // will not make a copy of it.
  // 
  // 应该使用长寿命（全局、静态、堆等）的 char* (i.e. __FILE__) 调用构造函数。它假
  // 定所提供的值将作为全局常量持续存在，并且不会复制它。
  Location(const char* function_name,
           const char* file_name,
           int line_number,
           const void* program_counter);

  // Provide a default constructor for easy of debugging.
  Location();

  // Comparison operator for insertion into a std::map<> hash tables.
  // All we need is *some* (any) hashing distinction.  Strings should already
  // be unique, so we don't bother with strcmp or such.
  // Use line number as the primary key (because it is fast, and usually gets us
  // a difference), and then pointers as secondary keys (just to get some
  // distinctions).
  // 
  // 用于插入到 std::map<> 容器中的比较运算符。使用 line_number_ 行号作为主键（因为
  // 它是快速的），然后 file_name_ 作为辅助键（只是为了得到一些区别）。
  bool operator < (const Location& other) const {
    if (line_number_ != other.line_number_)
      return line_number_ < other.line_number_;
    if (file_name_ != other.file_name_)
      return file_name_ < other.file_name_;
    return function_name_ < other.function_name_;
  }

  const char* function_name()   const { return function_name_; }
  const char* file_name()       const { return file_name_; }
  int line_number()             const { return line_number_; }
  const void* program_counter() const { return program_counter_; }

  std::string ToString() const;

  // Translate the some of the state in this instance into a human readable
  // string with HTML characters in the function names escaped, and append that
  // string to |output|.  Inclusion of the file_name_ and function_name_ are
  // optional, and controlled by the boolean arguments.
  // 
  // 将本实例的状态转换为人类可读字符串，其中 function_name_ 函数名转义了 HTML 为
  // 对应的实体字符，并将该字符串追加到 |output| 。是否包含 file_name_ 和 
  // function_name_ 是可选的，并由对应的布尔参数控制。
  void Write(bool display_filename, bool display_function_name,
             std::string* output) const;

  // Write function_name_ in HTML with '<' and '>' properly encoded.
  // 
  // 转化 function_name_ 中的 HTML('<' 和 '>') 代码为实体。
  void WriteFunctionName(std::string* output) const;

 private:
  const char* function_name_;
  const char* file_name_;
  int line_number_;
  const void* program_counter_; // 被调用函数上一级（调用者）退出地址。
};

// A "snapshotted" representation of the Location class that can safely be
// passed across process boundaries.
// 
// Location 的快照类 （包装类）
struct BUTIL_EXPORT LocationSnapshot {
  // The default constructor is exposed to support the IPC serialization macros.
  LocationSnapshot();
  explicit LocationSnapshot(const tracked_objects::Location& location);
  ~LocationSnapshot();

  std::string file_name;
  std::string function_name;
  int line_number;
};

BUTIL_EXPORT const void* GetProgramCounter();

// Define a macro to record the current source location.
// 
// 定义宏以记录当前源位置。
#define FROM_HERE FROM_HERE_WITH_EXPLICIT_FUNCTION(__FUNCTION__)

#define FROM_HERE_WITH_EXPLICIT_FUNCTION(function_name)                        \
    ::tracked_objects::Location(function_name,                                 \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                ::tracked_objects::GetProgramCounter())

}  // namespace tracked_objects

#endif  // BUTIL_LOCATION_H_

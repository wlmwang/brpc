// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/environment.h"

#include <vector>

#include "butil/strings/string_piece.h"
#include "butil/strings/string_util.h"
#include "butil/strings/utf_string_conversions.h"

#if defined(OS_POSIX)
#include <stdlib.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace butil {

namespace {

class EnvironmentImpl : public butil::Environment {
 public:
  virtual bool GetVar(const char* variable_name,
                      std::string* result) OVERRIDE {
    if (GetVarImpl(variable_name, result))
      return true;

    // Some commonly used variable names are uppercase while others
    // are lowercase, which is inconsistent. Let's try to be helpful
    // and look for a variable name with the reverse case.
    // I.e. HTTP_PROXY may be http_proxy for some users/systems.
    // 
    // 可能是一些常用的变量名称是大写，而另一些是小写的。因而此处尝试获取变量
    // 名大小写相反的情况，来避免因大小写不一致而导致的获取失败。
    // 如： HTTP_PROXY 可能是 http_proxy
    char first_char = variable_name[0];
    std::string alternate_case_var;
    if (first_char >= 'a' && first_char <= 'z')
      alternate_case_var = StringToUpperASCII(std::string(variable_name));
    else if (first_char >= 'A' && first_char <= 'Z')
      alternate_case_var = StringToLowerASCII(std::string(variable_name));
    else
      return false;
    return GetVarImpl(alternate_case_var.c_str(), result);
  }

  virtual bool SetVar(const char* variable_name,
                      const std::string& new_value) OVERRIDE {
    return SetVarImpl(variable_name, new_value);
  }

  virtual bool UnSetVar(const char* variable_name) OVERRIDE {
    return UnSetVarImpl(variable_name);
  }

 private:
  bool GetVarImpl(const char* variable_name, std::string* result) {
#if defined(OS_POSIX)
    const char* env_value = getenv(variable_name);
    if (!env_value)
      return false;
    // Note that the variable may be defined but empty.
    if (result)
      *result = env_value;
    return true;
#elif defined(OS_WIN)
    DWORD value_length = ::GetEnvironmentVariable(
        UTF8ToWide(variable_name).c_str(), NULL, 0);
    if (value_length == 0)
      return false;
    if (result) {
      scoped_ptr<wchar_t[]> value(new wchar_t[value_length]);
      ::GetEnvironmentVariable(UTF8ToWide(variable_name).c_str(), value.get(),
                               value_length);
      *result = WideToUTF8(value.get());
    }
    return true;
#else
#error need to port
#endif
  }

  bool SetVarImpl(const char* variable_name, const std::string& new_value) {
#if defined(OS_POSIX)
    // On success, zero is returned.
    // 
    // setenv() 成功返回 0
    return !setenv(variable_name, new_value.c_str(), 1);
#elif defined(OS_WIN)
    // On success, a nonzero value is returned.
    // 
    // SetEnvironmentVariable() 成功返回非 0
    return !!SetEnvironmentVariable(UTF8ToWide(variable_name).c_str(),
                                    UTF8ToWide(new_value).c_str());
#endif
  }

  bool UnSetVarImpl(const char* variable_name) {
#if defined(OS_POSIX)
    // On success, zero is returned.
    // 
    // unsetenv() 成功返回 0
    return !unsetenv(variable_name);
#elif defined(OS_WIN)
    // On success, a nonzero value is returned.
    // 
    // SetEnvironmentVariable() 成功返回非 0
    return !!SetEnvironmentVariable(UTF8ToWide(variable_name).c_str(), NULL);
#endif
  }
};

// Parses a null-terminated input string of an environment block. The key is
// placed into the given string, and the total length of the line, including
// the terminating null, is returned.
// 
// 解析以 null 结尾的输入字符串 |input| 的环境块("key=valye")。键写入 |key| 字符
// 串参数中，并返回该环境块的总长度，包括完整键值对和终止的空字符。
size_t ParseEnvLine(const NativeEnvironmentString::value_type* input,
                    NativeEnvironmentString* key) {
  // Skip to the equals or end of the string, this is the key.
  // 
  // 跳到字符串 "=" 尾部，用来获取一个 key
  size_t cur = 0;
  while (input[cur] && input[cur] != '=')
    cur++;
  *key = NativeEnvironmentString(&input[0], cur);

  // Now just skip to the end of the string.
  // 
  // 现在只需跳到字符串的末尾即可。
  while (input[cur])
    cur++;
  return cur + 1;
}

}  // namespace

namespace env_vars {

#if defined(OS_POSIX)
// On Posix systems, this variable contains the location of the user's home
// directory. (e.g, /home/username/).
// 
// 在 Posix 系统上，此变量包含用户主目录的位置 (e.g, /home/username/)
const char kHome[] = "HOME";
#endif

}  // namespace env_vars

Environment::~Environment() {}

// static
Environment* Environment::Create() {
  return new EnvironmentImpl();
}

bool Environment::HasVar(const char* variable_name) {
  return GetVar(variable_name, NULL);
}

#if defined(OS_WIN)

string16 AlterEnvironment(const wchar_t* env,
                          const EnvironmentMap& changes) {
  string16 result;

  // First copy all unmodified values to the output.
  size_t cur_env = 0;
  string16 key;
  while (env[cur_env]) {
    const wchar_t* line = &env[cur_env];
    size_t line_length = ParseEnvLine(line, &key);

    // Keep only values not specified in the change vector.
    EnvironmentMap::const_iterator found_change = changes.find(key);
    if (found_change == changes.end())
      result.append(line, line_length);

    cur_env += line_length;
  }

  // Now append all modified and new values.
  for (EnvironmentMap::const_iterator i = changes.begin();
       i != changes.end(); ++i) {
    if (!i->second.empty()) {
      result.append(i->first);
      result.push_back('=');
      result.append(i->second);
      result.push_back(0);
    }
  }

  // An additional null marks the end of the list. We always need a double-null
  // in case nothing was added above.
  if (result.empty())
    result.push_back(0);
  result.push_back(0);
  return result;
}

#elif defined(OS_POSIX)

scoped_ptr<char*[]> AlterEnvironment(const char* const* const env,
                                     const EnvironmentMap& changes) {
  // 保存连续的以 null 结尾的环境块字符串。
  std::string value_storage;  // Holds concatenated null-terminated strings.
  // 将连续环境块字符串 |value_storage| 中的每个环境块开始索引保存起来。
  std::vector<size_t> result_indices;  // Line indices into value_storage.

  // First build up all of the unchanged environment strings. These are
  // null-terminated of the form "key=value".
  // 
  // 首先构建所有不修改的环境字符串。这些是以 "key=value" 形式的空终止的字符串。
  std::string key;
  for (size_t i = 0; env[i]; i++) {
    // 解析一行环境变量键值对。
    size_t line_length = ParseEnvLine(env[i], &key);

    // Keep only values not specified in the change vector.
    // 
    // 仅保留修改(|changes|)数组中未指定的值。
    EnvironmentMap::const_iterator found_change = changes.find(key);
    if (found_change == changes.end()) {
      // 将连续环境块字符串 |value_storage| 中的每个环境块开始索引保存起来。
      result_indices.push_back(value_storage.size());
      // 将环境块 "key=value" 完整字符串写入 value_storage 中，以 null 结尾。
      value_storage.append(env[i], line_length);
    }
  }

  // Now append all modified and new values.
  // 
  // 现在附加所有修改(|changes|)的值和新值。
  for (EnvironmentMap::const_iterator i = changes.begin();
       i != changes.end(); ++i) {
    if (!i->second.empty()) {
      // 构造以 null 结尾的环境块 "key=value" 字符串。
      result_indices.push_back(value_storage.size());
      value_storage.append(i->first);
      value_storage.push_back('=');
      value_storage.append(i->second);
      value_storage.push_back(0); // 以 null 结尾
    }
  }

  // 申请一个足够存储所有环境块键值对(以 null 结尾字符串) + 每个环境块键值对开始
  // 地址(以 null 结尾)的内存。
  size_t pointer_count_required =
      result_indices.size() + 1 +  // Null-terminated array of pointers.
      (value_storage.size() + sizeof(char*) - 1) / sizeof(char*);  // Buffer.
  scoped_ptr<char*[]> result(new char*[pointer_count_required]);

  // The string storage goes after the array of pointers.
  // 
  // 所有环境块字符串存储位于 |result| 指针数组尾部。
  char* storage_data = reinterpret_cast<char*>(
      &result.get()[result_indices.size() + 1]);
  if (!value_storage.empty())
    memcpy(storage_data, value_storage.data(), value_storage.size());

  // Fill array of pointers at the beginning of the result.
  // 
  // |result| 指针数组的开头填充该指针数组中每个环境块的开始地址（构成 c-style 字
  // 符串）。
  for (size_t i = 0; i < result_indices.size(); i++)
    result[i] = &storage_data[result_indices[i]];
  result[result_indices.size()] = 0;  // Null terminator.

  return result.Pass();
}

#endif  // OS_POSIX

}  // namespace butil

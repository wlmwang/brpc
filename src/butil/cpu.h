// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUTIL_CPU_H_
#define BUTIL_CPU_H_

#include <string>

#include "butil/base_export.h"

namespace butil {

// Query information about the processor.
// 
// 查询有关处理器的信息
class BUTIL_EXPORT CPU {
 public:
  // Constructor
  CPU();

  // Intel 处理器架构
  enum IntelMicroArchitecture {
    PENTIUM,
    SSE,
    SSE2,
    SSE3,
    SSSE3,
    SSE41,
    SSE42,
    AVX,
    MAX_INTEL_MICRO_ARCHITECTURE
  };

  // Accessors for CPU information.
  // 
  // CPU 信息访问接口
  const std::string& vendor_name() const { return cpu_vendor_; }
  int signature() const { return signature_; }
  int stepping() const { return stepping_; }
  int model() const { return model_; }
  int family() const { return family_; }
  int type() const { return type_; }
  int extended_model() const { return ext_model_; }
  int extended_family() const { return ext_family_; }
  bool has_mmx() const { return has_mmx_; }
  bool has_sse() const { return has_sse_; }
  bool has_sse2() const { return has_sse2_; }
  bool has_sse3() const { return has_sse3_; }
  bool has_ssse3() const { return has_ssse3_; }
  bool has_sse41() const { return has_sse41_; }
  bool has_sse42() const { return has_sse42_; }
  bool has_avx() const { return has_avx_; }
  // has_avx_hardware returns true when AVX is present in the CPU. This might
  // differ from the value of |has_avx()| because |has_avx()| also tests for
  // operating system support needed to actually call AVX instuctions.
  // Note: you should never need to call this function. It was added in order
  // to workaround a bug in NSS but |has_avx()| is what you want.
  // 
  // 当 AVX 存在于 CPU 中时，has_avx_hardware 返回 true。 这可能与 |has_avx()| 的值不同。
  // 因为 |has_avx()| 还测试实际调用 AVX 指令所需的操作系统支持。
  // 注意：你永远不需要调用这个函数。它是为了解决 NSS 中的一个错误而添加的，但 |has_avx()| 是
  // 你想要的。
  bool has_avx_hardware() const { return has_avx_hardware_; }
  bool has_aesni() const { return has_aesni_; }
  bool has_non_stop_time_stamp_counter() const {
    return has_non_stop_time_stamp_counter_;
  }
  IntelMicroArchitecture GetIntelMicroArchitecture() const;
  const std::string& cpu_brand() const { return cpu_brand_; }

 private:
  // Query the processor for CPUID information.
  void Initialize();

  int signature_;  // raw form of type, family, model, and stepping
  int type_;  // process type
  int family_;  // family of the processor
  int model_;  // model of processor
  int stepping_;  // processor revision number
  int ext_model_;
  int ext_family_;
  bool has_mmx_;
  bool has_sse_;
  bool has_sse2_;
  bool has_sse3_;
  bool has_ssse3_;
  bool has_sse41_;
  bool has_sse42_;
  bool has_avx_;
  bool has_avx_hardware_;
  bool has_aesni_;
  bool has_non_stop_time_stamp_counter_;
  std::string cpu_vendor_;
  std::string cpu_brand_;
};

}  // namespace butil

#endif  // BUTIL_CPU_H_

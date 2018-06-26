// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/memory/aligned_memory.h"

#include "butil/logging.h"

#if defined(OS_ANDROID)
#include <malloc.h>
#endif

namespace butil {

// 对齐方式申请内存资源
void* AlignedAlloc(size_t size, size_t alignment) {
  DCHECK_GT(size, 0U);  // size>0
  DCHECK_EQ(alignment & (alignment - 1), 0U); // 2 幂次方
  DCHECK_EQ(alignment % sizeof(void*), 0U);   // 指针长度倍数
  void* ptr = NULL;
#if defined(COMPILER_MSVC)
  ptr = _aligned_malloc(size, alignment);
// Android technically supports posix_memalign(), but does not expose it in
// the current version of the library headers used by Chrome.  Luckily,
// memalign() on Android returns pointers which can safely be used with
// free(), so we can use it instead.  Issue filed to document this:
// http://code.google.com/p/android/issues/detail?id=35391
// 
// Android 技术上支持 posix_memalign() ，但不会在 Chrome 当前版本中使用它。幸运的是，
// Android 上的 memalign() 返回可以安全地用于 free() 的指针，所以我们可以使用它。
#elif defined(OS_ANDROID)
  ptr = memalign(alignment, size);
#else
  if (posix_memalign(&ptr, alignment, size))
    ptr = NULL;
#endif
  // Since aligned allocations may fail for non-memory related reasons, force a
  // crash if we encounter a failed allocation; maintaining consistent behavior
  // with a normal allocation failure in Chrome.
  // 
  // 由于对齐的分配可能因非内存相关的原因而失败，如果我们遇到分配失败，则强制崩溃；在 Chrome 
  // 中保持一致的分配失败行为。
  if (!ptr) {
    DLOG(ERROR) << "If you crashed here, your aligned allocation is incorrect: "
                << "size=" << size << ", alignment=" << alignment;
    CHECK(false);
  }
  // Sanity check alignment just to be safe.
  // 
  // 对齐检查校准
  DCHECK_EQ(reinterpret_cast<uintptr_t>(ptr) & (alignment - 1), 0U);
  return ptr;
}

}  // namespace butil

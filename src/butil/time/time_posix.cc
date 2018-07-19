// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "butil/time/time.h"

#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#if defined(OS_ANDROID) && !defined(__LP64__)
#include <time64.h>
#endif
#include <unistd.h>

#include <limits>
#include <ostream>

#include "butil/basictypes.h"
#include "butil/logging.h"
#include "butil/port.h"
#include "butil/build_config.h"

#if defined(OS_ANDROID)
#include "butil/os_compat_android.h"
#elif defined(OS_NACL)
#include "butil/os_compat_nacl.h"
#endif

namespace {

// @tips
// timegm 函数只是将 struct tm 结构转成 time_t 结构，不使用时区信息。即，比 mktime 
// 少了一个解析 timezone 文件的操作，从而也就比其快很多。timegm 函数（不是 posix 标
// 准，但 glibc 的也有实现 bsd 接口）。
// mktime 会使用时区信息。
// 
// gmtime 函数只是将 time_t 结构转成 struct tm 结构，不使用时区信息。
// localtime 会使用时区信息。
// 
// 文件的修改时间等信息全部采用 GMT 时间存放，不同的系统在得到修改时间后通过 localtime 
// 转换成本地时间。

#if !defined(OS_MACOSX)
// Define a system-specific SysTime that wraps either to a time_t or
// a time64_t depending on the host system, and associated convertion.
// See crbug.com/162007
#if defined(OS_ANDROID) && !defined(__LP64__)
typedef time64_t SysTime;

SysTime SysTimeFromTimeStruct(struct tm* timestruct, bool is_local) {
  if (is_local)
    return mktime64(timestruct);
  else
    return timegm64(timestruct);
}

void SysTimeToTimeStruct(SysTime t, struct tm* timestruct, bool is_local) {
  if (is_local)
    localtime64_r(&t, timestruct);
  else
    gmtime64_r(&t, timestruct);
}

#else  // OS_ANDROID && !__LP64__
typedef time_t SysTime;

SysTime SysTimeFromTimeStruct(struct tm* timestruct, bool is_local) {
  if (is_local)
    return mktime(timestruct);
  else
    return timegm(timestruct);
}

void SysTimeToTimeStruct(SysTime t, struct tm* timestruct, bool is_local) {
  if (is_local)
    localtime_r(&t, timestruct);
  else
    gmtime_r(&t, timestruct);
}
#endif  // OS_ANDROID

// Helper function to get results from clock_gettime() as TimeTicks object.
// Minimum requirement is MONOTONIC_CLOCK to be supported on the system.
// FreeBSD 6 has CLOCK_MONOTONIC but defines _POSIX_MONOTONIC_CLOCK to -1.
// 
// 
// @tips
// \file <time.h>
// int clock_gettime(clockid_t clk_id, struct timespec *tp);
// POSIX 平台提供的标准计时器 API 的封装。
// 
// CLOCK_REALTIME: 这种类型的时钟可以反映 wall clock time ，用的是绝对时间，
//    当系统的时钟源被改变，或者系统管理员重置了系统时间之后，这种类型的时钟可以
//    得到相应的调整，也就是说，系统时间影响这种类型的 timer 。
// CLOCK_MONOTONIC: 用的是相对时间，他的时间是通过 jiffies 值来计算的。该时钟
//    不受系统时钟源的影响，只受 jiffies 值的影响。
// CLOCK_PROCESS_CPUTIME_ID: 本进程到当前代码系统 CPU 花费的时间。
// CLOCK_THREAD_CPUTIME_ID: 本线程到当前代码系统 CPU 花费的时间。
// 
// 从 clock_gettime() 获取结果作为 TimeTicks 对象的辅助函数。
// 最低要求是系统支持 MONOTONIC_CLOCK 。FreeBSD 6 有 CLOCK_MONOTONIC 但是 
// _POSIX_MONOTONIC_CLOCK 定义为 -1 。
#if (defined(OS_POSIX) &&                                               \
     defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK >= 0) || \
    defined(OS_BSD) || defined(OS_ANDROID)
butil::TimeTicks ClockNow(clockid_t clk_id) {
  uint64_t absolute_micro;

  struct timespec ts;
  if (clock_gettime(clk_id, &ts) != 0) {
    NOTREACHED() << "clock_gettime(" << clk_id << ") failed.";
    return butil::TimeTicks();
  }

  absolute_micro =
      (static_cast<int64_t>(ts.tv_sec) * butil::Time::kMicrosecondsPerSecond) +
      (static_cast<int64_t>(ts.tv_nsec) / butil::Time::kNanosecondsPerMicrosecond);

  return butil::TimeTicks::FromInternalValue(absolute_micro);
}
#else  // _POSIX_MONOTONIC_CLOCK
#error No usable tick clock function on this platform.
#endif  // _POSIX_MONOTONIC_CLOCK
#endif  // !defined(OS_MACOSX)

}  // namespace

namespace butil {

struct timespec TimeDelta::ToTimeSpec() const {
  int64_t microseconds = InMicroseconds();
  time_t seconds = 0;
  if (microseconds >= Time::kMicrosecondsPerSecond) {
    seconds = InSeconds();
    microseconds -= seconds * Time::kMicrosecondsPerSecond;
  }
  struct timespec result =
      {seconds,
       static_cast<long>(microseconds * Time::kNanosecondsPerMicrosecond)};
  return result;
}

#if !defined(OS_MACOSX)
// The Time routines in this file use standard POSIX routines, or almost-
// standard routines in the case of timegm.  We need to use a Mach-specific
// function for TimeTicks::Now() on Mac OS X.

// Time -----------------------------------------------------------------------

// Windows uses a Gregorian epoch of 1601.  We need to match this internally
// so that our time representations match across all platforms.  See bug 14734.
//   irb(main):010:0> Time.at(0).getutc()
//   => Thu Jan 01 00:00:00 UTC 1970
//   irb(main):011:0> Time.at(-11644473600).getutc()
//   => Mon Jan 01 00:00:00 UTC 1601
static const int64_t kWindowsEpochDeltaSeconds = GG_INT64_C(11644473600);

// static
const int64_t Time::kWindowsEpochDeltaMicroseconds =
    kWindowsEpochDeltaSeconds * Time::kMicrosecondsPerSecond;

// Some functions in time.cc use time_t directly, so we provide an offset
// to convert from time_t (Unix epoch) and internal (Windows epoch).
// static
const int64_t Time::kTimeTToMicrosecondsOffset = kWindowsEpochDeltaMicroseconds;

// static
Time Time::Now() {
  struct timeval tv;
  struct timezone tz = { 0, 0 };  // UTC
  if (gettimeofday(&tv, &tz) != 0) {
    DCHECK(0) << "Could not determine time of day";
    PLOG(ERROR) << "Call to gettimeofday failed.";
    // Return null instead of uninitialized |tv| value, which contains random
    // garbage data. This may result in the crash seen in crbug.com/147570.
    // 
    // 返回 null 而不是未初始化的 |tv| 值，其中包含随机垃圾数据。
    return Time();
  }
  // Combine seconds and microseconds in a 64-bit field containing microseconds
  // since the epoch.  That's enough for nearly 600 centuries.  Adjust from
  // Unix (1970) to Windows (1601) epoch.
  // 
  // 自时代以来，所“走过”的微秒 64-bit 整型值。这已经足够近 600 个世纪了。函数包含了，
  // 从 Unix (1970) 调整到 Windows (1601) 时代的逻辑。
  return Time((tv.tv_sec * kMicrosecondsPerSecond + tv.tv_usec) +
      kWindowsEpochDeltaMicroseconds);
}

// static
Time Time::NowFromSystemTime() {
  // Just use Now() because Now() returns the system time.
  return Now();
}

void Time::Explode(bool is_local, Exploded* exploded) const {
  // Time stores times with microsecond resolution, but Exploded only carries
  // millisecond resolution, so begin by being lossy.  Adjust from Windows
  // epoch (1601) to Unix epoch (1970);
  // 
  // Time 以微秒分辨率存储时间，但 Exploded 仅存在毫秒分辨率，因此从有损开始。
  // 从 Windows epoch(1601) 调整到 Unix epoch (1970) 。
  int64_t microseconds = us_ - kWindowsEpochDeltaMicroseconds;
  // The following values are all rounded towards -infinity.
  int64_t milliseconds;  // Milliseconds since epoch.
  SysTime seconds;  // Seconds since epoch.
  int millisecond;  // Exploded millisecond value (0-999).
  if (microseconds >= 0) {
    // Rounding towards -infinity <=> rounding towards 0, in this case.
    milliseconds = microseconds / kMicrosecondsPerMillisecond;
    seconds = milliseconds / kMillisecondsPerSecond;
    millisecond = milliseconds % kMillisecondsPerSecond;
  } else {
    // Round these *down* (towards -infinity).
    milliseconds = (microseconds - kMicrosecondsPerMillisecond + 1) /
                   kMicrosecondsPerMillisecond;
    seconds = (milliseconds - kMillisecondsPerSecond + 1) /
              kMillisecondsPerSecond;
    // Make this nonnegative (and between 0 and 999 inclusive).
    millisecond = milliseconds % kMillisecondsPerSecond;
    if (millisecond < 0)
      millisecond += kMillisecondsPerSecond;
  }

  struct tm timestruct;
  SysTimeToTimeStruct(seconds, &timestruct, is_local);

  exploded->year         = timestruct.tm_year + 1900;
  exploded->month        = timestruct.tm_mon + 1;
  exploded->day_of_week  = timestruct.tm_wday;
  exploded->day_of_month = timestruct.tm_mday;
  exploded->hour         = timestruct.tm_hour;
  exploded->minute       = timestruct.tm_min;
  exploded->second       = timestruct.tm_sec;
  exploded->millisecond  = millisecond;
}

// static
Time Time::FromExploded(bool is_local, const Exploded& exploded) {
  struct tm timestruct;
  timestruct.tm_sec    = exploded.second;
  timestruct.tm_min    = exploded.minute;
  timestruct.tm_hour   = exploded.hour;
  timestruct.tm_mday   = exploded.day_of_month;
  timestruct.tm_mon    = exploded.month - 1;
  timestruct.tm_year   = exploded.year - 1900;
  timestruct.tm_wday   = exploded.day_of_week;  // mktime/timegm ignore this
  timestruct.tm_yday   = 0;     // mktime/timegm ignore this
  timestruct.tm_isdst  = -1;    // attempt to figure it out
#if !defined(OS_NACL) && !defined(OS_SOLARIS)
  timestruct.tm_gmtoff = 0;     // not a POSIX field, so mktime/timegm ignore
  timestruct.tm_zone   = NULL;  // not a POSIX field, so mktime/timegm ignore
#endif


  int64_t milliseconds;
  SysTime seconds;

  // Certain exploded dates do not really exist due to daylight saving times,
  // and this causes mktime() to return implementation-defined values when
  // tm_isdst is set to -1. On Android, the function will return -1, while the
  // C libraries of other platforms typically return a liberally-chosen value.
  // Handling this requires the special code below.
  // 
  // 由于夏时令机制，某些 exploded 可能并不存在(比如，1987/4/12 00:00:00 在中国的本地
  // 时间中就是不存在的)，并且会导致 mktime() 在 tm_isdst 设置为 -1 时返回实现定义的值。
  // 在 Android 上，该函数将返回 -1 ，而其他平台的 C 库通常返回一个自由选择的值。处理此
  // 问题需要以下特殊代码。

  // SysTimeFromTimeStruct() modifies the input structure, save current value.
  struct tm timestruct0 = timestruct;

  seconds = SysTimeFromTimeStruct(&timestruct, is_local);
  if (seconds == -1) {
    // Get the time values with tm_isdst == 0 and 1, then select the closest one
    // to UTC 00:00:00 that isn't -1.
    // 
    // 使用 tm_isdst == 0/1 获取时间值，然后选择最接近 UTC 00:00:00 且不是 -1 的值。
    timestruct = timestruct0;
    timestruct.tm_isdst = 0;
    int64_t seconds_isdst0 = SysTimeFromTimeStruct(&timestruct, is_local);

    timestruct = timestruct0;
    timestruct.tm_isdst = 1;
    int64_t seconds_isdst1 = SysTimeFromTimeStruct(&timestruct, is_local);

    // seconds_isdst0 or seconds_isdst1 can be -1 for some timezones.
    // E.g. "CLST" (Chile Summer Time) returns -1 for 'tm_isdt == 1'.
    // 
    // 对于某些时区， seconds_isdst0 或 seconds_isdst1 可以是 -1 。例如： "CLST"（智利
    // 夏令时）为 'tm_isdt == 1' 返回 -1 。
    if (seconds_isdst0 < 0)
      seconds = seconds_isdst1;
    else if (seconds_isdst1 < 0)
      seconds = seconds_isdst0;
    else
      seconds = std::min(seconds_isdst0, seconds_isdst1);
  }

  // Handle overflow.  Clamping the range to what mktime and timegm might
  // return is the best that can be done here.  It's not ideal, but it's better
  // than failing here or ignoring the overflow case and treating each time
  // overflow as one second prior to the epoch.
  // 
  // 处理溢出。将范围限制在 mktime 和 timegm 可能返回的范围内是可以在此处完成的最佳选择。
  // 这不是理想的，但它比在这里失败或忽略溢出情况更好，并且每次溢出都是在纪元之前的一秒钟处
  // 理。
  if (seconds == -1 &&
      (exploded.year < 1969 || exploded.year > 1970)) {
    // If exploded.year is 1969 or 1970, take -1 as correct, with the
    // time indicating 1 second prior to the epoch.  (1970 is allowed to handle
    // time zone and DST offsets.)  Otherwise, return the most future or past
    // time representable.  Assumes the time_t epoch is 1970-01-01 00:00:00 UTC.
    //
    // The minimum and maximum representible times that mktime and timegm could
    // return are used here instead of values outside that range to allow for
    // proper round-tripping between exploded and counter-type time
    // representations in the presence of possible truncation to time_t by
    // division and use with other functions that accept time_t.
    //
    // When representing the most distant time in the future, add in an extra
    // 999ms to avoid the time being less than any other possible value that
    // this function can return.
    // 
    // 如果 exploded.year 是 1969 年或 1970 年，则 -1 作为正确的，时间表示在该纪元前 1 秒。
    // （1970 年允许被处理时区和 DST 偏移量。）否则，返回最后的或过去的时间可表示的。假设 time_t 
    // 纪元是 1970-01-01 00:00:00 UTC 。
    // 
    // 在这里使用 mktime 和 timegm 可以返回的最小和最大可表示时间，而不是该范围之外的值，以
    // 允许在可能的截断到 time_t 的情况下通过分割和与其他的使用之间在分解和计数型时间表示之间
    // 进行适当的往返 接受 time_t 的函数。
    // 
    // 当代表将来最遥远的时间时，添加额外的 999ms 以避免时间小于此函数可返回的任何其他可能值。

    // On Android, SysTime is int64_t, special care must be taken to avoid
    // overflows.
    // 
    // 在 Android 上， SysTime 是 int64_t ，必须特别注意避免溢出。
    const int64_t min_seconds = (sizeof(SysTime) < sizeof(int64_t))
                                  ? std::numeric_limits<SysTime>::min()
                                  : std::numeric_limits<int32_t>::min();
    const int64_t max_seconds = (sizeof(SysTime) < sizeof(int64_t))
                                  ? std::numeric_limits<SysTime>::max()
                                  : std::numeric_limits<int32_t>::max();
    if (exploded.year < 1969) {
      milliseconds = min_seconds * kMillisecondsPerSecond;
    } else {
      milliseconds = max_seconds * kMillisecondsPerSecond;
      milliseconds += (kMillisecondsPerSecond - 1);
    }
  } else {
    milliseconds = seconds * kMillisecondsPerSecond + exploded.millisecond;
  }

  // Adjust from Unix (1970) to Windows (1601) epoch.
  return Time((milliseconds * kMicrosecondsPerMillisecond) +
      kWindowsEpochDeltaMicroseconds);
}

// TimeTicks ------------------------------------------------------------------
// static
TimeTicks TimeTicks::Now() {
  // 获得 POSIX 相对时钟（相比 CLOCK_REALTIME 实时时钟，它不会被复位和调整）。
  return ClockNow(CLOCK_MONOTONIC);
}

// static
TimeTicks TimeTicks::HighResNow() {
  return Now();
}

// static
bool TimeTicks::IsHighResNowFastAndReliable() {
  return true;
}

// static
TimeTicks TimeTicks::ThreadNow() {
#if (defined(_POSIX_THREAD_CPUTIME) && (_POSIX_THREAD_CPUTIME >= 0)) || \
    defined(OS_ANDROID)
  return ClockNow(CLOCK_THREAD_CPUTIME_ID);
#else
  NOTREACHED();
  return TimeTicks();
#endif
}

// Use the Chrome OS specific system-wide clock.
#if defined(OS_CHROMEOS)
// static
TimeTicks TimeTicks::NowFromSystemTraceTime() {
  uint64_t absolute_micro;

  struct timespec ts;
  if (clock_gettime(kClockSystemTrace, &ts) != 0) {
    // NB: fall-back for a chrome os build running on linux
    return HighResNow();
  }

  absolute_micro =
      (static_cast<int64_t>(ts.tv_sec) * Time::kMicrosecondsPerSecond) +
      (static_cast<int64_t>(ts.tv_nsec) / Time::kNanosecondsPerMicrosecond);

  return TimeTicks(absolute_micro);
}

#else  // !defined(OS_CHROMEOS)

// static
TimeTicks TimeTicks::NowFromSystemTraceTime() {
  return HighResNow();
}

#endif  // defined(OS_CHROMEOS)

#endif  // !OS_MACOSX

// static
Time Time::FromTimeVal(struct timeval t) {
  DCHECK_LT(t.tv_usec, static_cast<int>(Time::kMicrosecondsPerSecond));
  DCHECK_GE(t.tv_usec, 0);
  if (t.tv_usec == 0 && t.tv_sec == 0)
    return Time();
  if (t.tv_usec == static_cast<suseconds_t>(Time::kMicrosecondsPerSecond) - 1 &&
      t.tv_sec == std::numeric_limits<time_t>::max())
    return Max();
  return Time(
      (static_cast<int64_t>(t.tv_sec) * Time::kMicrosecondsPerSecond) +
      t.tv_usec +
      kTimeTToMicrosecondsOffset);
}

struct timeval Time::ToTimeVal() const {
  struct timeval result;
  if (is_null()) {
    result.tv_sec = 0;
    result.tv_usec = 0;
    return result;
  }
  if (is_max()) {
    result.tv_sec = std::numeric_limits<time_t>::max();
    result.tv_usec = static_cast<suseconds_t>(Time::kMicrosecondsPerSecond) - 1;
    return result;
  }
  int64_t us = us_ - kTimeTToMicrosecondsOffset;
  result.tv_sec = us / Time::kMicrosecondsPerSecond;
  result.tv_usec = us % Time::kMicrosecondsPerSecond;
  return result;
}

}  // namespace butil

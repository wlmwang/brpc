// Copyright (c) 2010 Baidu, Inc.
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
// Date: Wed Aug 11 10:38:17 2010

// Measuring time

#ifndef BUTIL_BAIDU_TIME_H
#define BUTIL_BAIDU_TIME_H

#include <time.h>                            // timespec, clock_gettime
#include <sys/time.h>                        // timeval, gettimeofday
#include <stdint.h>                          // int64_t, uint64_t

#if defined(NO_CLOCK_GETTIME_IN_MAC)
#include <mach/mach.h>
# define CLOCK_REALTIME CALENDAR_CLOCK
# define CLOCK_MONOTONIC SYSTEM_CLOCK

typedef int clockid_t;

// clock_gettime is not available in MacOS < 10.12
int clock_gettime(clockid_t id, timespec* time);

#endif

// @tips
// \file <sys/time.h>
// typedef long time_t;
// 一般用来表示从 1970-01-01 00:00:00 时以来的秒数，精确度：秒。
// 可由函数 time_t time(time_t* lpt); 获取
// Use like:
// time_t time = time(NULL);
// 
// -----------------------------------------------------------------
// \file <sys/timeb.h>
// struct timeb
// {
//      time_t time; 
//      unsigned short millitm; 
//      short timezone; 
//      short dstflag; 
// };
// 它有两个主要成员，一个是秒，另一个是毫秒；精确度：毫秒(10E-3秒)。
// 可由函数 int ftime(struct timeb* tp); 获取 struct timeb 结构的时间
// 
// -------------------------------------------------------------------------
// \file <sys/time.h>
// struct timeval 
// {
//    long tv_sec; /* seconds */
//    long tv_usec; /* microseconds */
// };
// 有两个成员，一个是秒，一个是微妙，精确度：微秒(10E-6)；
// 可由函数 int gettimeofday(struct timeval *tv, struct timezone *tz); 获取
// 
// struct timezone
// {
//   int tz_minuteswest; /* 和 Greewich 时间差了多少分钟*/
//   int tz_dsttime; /* 日光节约时间的状态 */
// };
// 
// 设置特定时钟的时间：
// long settimeofday(struct timeval *tv ,struct timezone *tz);
// 
// ---------------------------------------------------------------------
// \file <time.h>
// struct timespec
// {
//    time_t tv_sec; /* seconds */
//    long tv_nsec; /* nanoseconds */
// };
// 
// 有两个成员，一个是秒，一个是纳秒, 精确度：纳秒(10E-9秒)；
// 一般由函数 long clock_gettime(clockid_t __clock_id, struct timespec *tp); 获取
// 获取特定时钟的时间，时间通过 tp 结构传回，目前定义了6种 clockid_t ，分别是
// CLOCK_REALTIME             系统实时时间，随系统实时时间改变而改变。
//                            即从 UTC 1970-1-1 0:0:0 开始计时，中间时刻如果系统时间
//                            被用户改成其他，则对应的时间相应改变。
// CLOCK_MONOTONIC            从系统启动一刻起开始计时，不受系统时间被用户改变的影响。
// CLOCK_PROCESS_CPUTIME_ID   本进程到当前代码系统 CPU 花费的时间。
// CLOCK_THREAD_CPUTIME_ID    本线程到当前代码系统 CPU 花费的时间。
// CLOCK_REALTIME_HR          CLOCK_REALTIME 的高精度版本。
// CLOCK_MONOTONIC_HR         CLOCK_MONOTONIC 的高精度版本。
// 
// 设置特定时钟的时间：
// long clock_settime(clockid_t ,struct timespec*);
// 
// -----------------------------------------------------------------------
// \file <time.h>
// typedef long clock_t;
// 该函数以微秒的方式返回 CPU 的时间
// 可由函数 clock_t clock(void); 获得
// 
// -------------------------------------------------------------------------
// \file <time.h>
// struct tm
// {
//     int tm_sec;  // 目前秒数，正常范围为0-59，但允许至61秒
//     int tm_min;  // 目前分数，范围0-59
//     int tm_hour; // 从午夜算起的时数，范围为0-23 
//     int tm_mday; // 目前月份的日数，范围01-31 
//     int tm_mon;  // 目前月份，从一月算起，范围从0-11
//     int tm_year; // 1900 年算起至今的年数
//     int tm_wday; // 一星期的日数，从星期一算起，范围为0-6
//     int tm_yday; // 从今年1月1日算起至今的天数，范围为0-365
//     int tm_isdst;// 日光节约时间的旗标
// };
// 可由函数 struct tm* gmtime(const time_t* timep); 获得
// 该将参数 timep 所指的 time_t 结构中的信息转换成真实世界所使用的时间日期表示方法，然后将
// 结果由结构 tm 返回

namespace butil {

// Get SVN revision of this copy.
const char* last_changed_revision();

// ----------------------
// timespec manipulations
// ----------------------

// Let tm->tv_nsec be in [0, 1,000,000,000) if it's not.
// 
// 格式化时间 struct timespec
inline void timespec_normalize(timespec* tm) {
    if (tm->tv_nsec >= 1000000000L) {
        const int64_t added_sec = tm->tv_nsec / 1000000000L;
        tm->tv_sec += added_sec;
        tm->tv_nsec -= added_sec * 1000000000L;
    } else if (tm->tv_nsec < 0) {
        const int64_t sub_sec = (tm->tv_nsec - 999999999L) / 1000000000L;
        tm->tv_sec += sub_sec;
        tm->tv_nsec -= sub_sec * 1000000000L;
    }
}

// Add timespec |span| into timespec |*tm|.
// 
// 时间之和，输出到参数 tm
inline void timespec_add(timespec *tm, const timespec& span) {
    tm->tv_sec += span.tv_sec;
    tm->tv_nsec += span.tv_nsec;
    timespec_normalize(tm);
}

// Minus timespec |span| from timespec |*tm|. 
// tm->tv_nsec will be inside [0, 1,000,000,000)
// 
// 时间只差，输出到参数 tm
inline void timespec_minus(timespec *tm, const timespec& span) {
    tm->tv_sec -= span.tv_sec;
    tm->tv_nsec -= span.tv_nsec;
    timespec_normalize(tm);
}

// ------------------------------------------------------------------
// Get the timespec after specified duration from |start_time|
// ------------------------------------------------------------------
// 
// 返回指定时间点 start_time 的 nanoseconds 纳秒后的时间点
inline timespec nanoseconds_from(timespec start_time, int64_t nanoseconds) {
    start_time.tv_nsec += nanoseconds;
    timespec_normalize(&start_time);
    return start_time;
}

inline timespec microseconds_from(timespec start_time, int64_t microseconds) {
    return nanoseconds_from(start_time, microseconds * 1000L);
}

inline timespec milliseconds_from(timespec start_time, int64_t milliseconds) {
    return nanoseconds_from(start_time, milliseconds * 1000000L);
}

inline timespec seconds_from(timespec start_time, int64_t seconds) {
    return nanoseconds_from(start_time, seconds * 1000000000L);
}

// --------------------------------------------------------------------
// Get the timespec after specified duration from now (CLOCK_REALTIME)
// --------------------------------------------------------------------
// 
// 返回距离当前时间点 nanoseconds 纳秒后的时间点
inline timespec nanoseconds_from_now(int64_t nanoseconds) {
    timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return nanoseconds_from(time, nanoseconds);
}

inline timespec microseconds_from_now(int64_t microseconds) {
    return nanoseconds_from_now(microseconds * 1000L);
}

inline timespec milliseconds_from_now(int64_t milliseconds) {
    return nanoseconds_from_now(milliseconds * 1000000L);
}

inline timespec seconds_from_now(int64_t seconds) {
    return nanoseconds_from_now(seconds * 1000000000L);
}

inline timespec timespec_from_now(const timespec& span) {
    timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    timespec_add(&time, span);
    return time;
}

// ---------------------------------------------------------------------
// Convert timespec to and from a single integer.
// For conversions between timespec and timeval, use TIMEVAL_TO_TIMESPEC
// and TIMESPEC_TO_TIMEVAL defined in <sys/time.h>
// ---------------------------------------------------------------------1
// 
// 转换 timespec 时间的纳秒整型值
inline int64_t timespec_to_nanoseconds(const timespec& ts) {
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

inline int64_t timespec_to_microseconds(const timespec& ts) {
    return timespec_to_nanoseconds(ts) / 1000L;
}

inline int64_t timespec_to_milliseconds(const timespec& ts) {
    return timespec_to_nanoseconds(ts) / 1000000L;
}

inline int64_t timespec_to_seconds(const timespec& ts) {
    return timespec_to_nanoseconds(ts) / 1000000000L;
}

// 纳秒转换成 timespec 时间
inline timespec nanoseconds_to_timespec(int64_t ns) {
    timespec ts;
    ts.tv_sec = ns / 1000000000L;
    ts.tv_nsec = ns - ts.tv_sec * 1000000000L;
    return ts;
}

inline timespec microseconds_to_timespec(int64_t us) {
    return nanoseconds_to_timespec(us * 1000L);
}

inline timespec milliseconds_to_timespec(int64_t ms) {
    return nanoseconds_to_timespec(ms * 1000000L);
}

inline timespec seconds_to_timespec(int64_t s) {
    return nanoseconds_to_timespec(s * 1000000000L);
}

// ---------------------------------------------------------------------
// Convert timeval to and from a single integer.                                             
// For conversions between timespec and timeval, use TIMEVAL_TO_TIMESPEC
// and TIMESPEC_TO_TIMEVAL defined in <sys/time.h>
// ---------------------------------------------------------------------
// 
// timeval 转成微妙值
inline int64_t timeval_to_microseconds(const timeval& tv) {
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

inline int64_t timeval_to_milliseconds(const timeval& tv) {
    return timeval_to_microseconds(tv) / 1000L;
}

inline int64_t timeval_to_seconds(const timeval& tv) {
    return timeval_to_microseconds(tv) / 1000000L;
}

// 微妙转换成 timeval 时间
inline timeval microseconds_to_timeval(int64_t us) {
    timeval tv;
    tv.tv_sec = us / 1000000L;
    tv.tv_usec = us - tv.tv_sec * 1000000L;
    return tv;
}

inline timeval milliseconds_to_timeval(int64_t ms) {
    return microseconds_to_timeval(ms * 1000L);
}

inline timeval seconds_to_timeval(int64_t s) {
    return microseconds_to_timeval(s * 1000000L);
}

// ---------------------------------------------------------------
// Get system-wide monotonic time.
// ---------------------------------------------------------------
// 
// 获取系统单调时间：系统启动以后流逝的时间。这是由变量 jiffies 来记录的。
// 系统每次启动时 jiffies 初始化为 0，每来一个 timer interrupt, jiffies+=1,
// 也就是说它代表系统启动后流逝的 tick 数
extern int64_t monotonic_time_ns();

inline int64_t monotonic_time_us() { 
    return monotonic_time_ns() / 1000L; 
}

inline int64_t monotonic_time_ms() {
    return monotonic_time_ns() / 1000000L; 
}

inline int64_t monotonic_time_s() {
    return monotonic_time_ns() / 1000000000L;
}

namespace detail {
// 读取 rtc 时间
inline uint64_t clock_cycles() {
    unsigned int lo = 0;
    unsigned int hi = 0;
    // We cannot use "=A", since this would use %rax on x86_64
    __asm__ __volatile__ (
        "rdtsc"
        : "=a" (lo), "=d" (hi)
        );
    return ((uint64_t)hi << 32) | lo;
}
extern int64_t read_invariant_cpu_frequency();
// Be positive iff:
// 1 Intel x86_64 CPU (multiple cores) supporting constant_tsc and
// nonstop_tsc(check flags in /proc/cpuinfo)
extern int64_t invariant_cpu_freq;
}  // namespace detail

// ---------------------------------------------------------------
// Get cpu-wide (wall-) time.
// Cost ~9ns on Intel(R) Xeon(R) CPU E5620 @ 2.40GHz
// ---------------------------------------------------------------
// note: Inlining shortens time cost per-call for 15ns in a loop of many
//       calls to this function.
//       
// 获取挂钟时间：现实的时间。这是由变量 xtime 来记录的。系统每次启动时将 CMOS 上
// 的 RTC 时间读入 xtime. 即自 1970-01-01 起经历的秒数、本秒中经历的纳秒数
inline int64_t cpuwide_time_ns() {
    if (detail::invariant_cpu_freq > 0) {
        const uint64_t tsc = detail::clock_cycles();
        const uint64_t sec = tsc / detail::invariant_cpu_freq;
        // TODO: should be OK until CPU's frequency exceeds 16GHz.
        return (tsc - sec * detail::invariant_cpu_freq) * 1000000000L /
            detail::invariant_cpu_freq + sec * 1000000000L;
    } else if (!detail::invariant_cpu_freq) {
        // Lack of necessary features, return system-wide monotonic time instead.
        return monotonic_time_ns();
    } else {
        // Use a thread-unsafe method(OK to us) to initialize the freq
        // to save a "if" test comparing to using a local static variable
        detail::invariant_cpu_freq = detail::read_invariant_cpu_frequency();
        return cpuwide_time_ns();
    }
}

inline int64_t cpuwide_time_us() {
    return cpuwide_time_ns() / 1000L;
}

inline int64_t cpuwide_time_ms() { 
    return cpuwide_time_ns() / 1000000L;
}

inline int64_t cpuwide_time_s() {
    return cpuwide_time_ns() / 1000000000L;
}

// --------------------------------------------------------------------
// Get elapse since the Epoch.                                          
// No gettimeofday_ns() because resolution of timeval is microseconds.  
// Cost ~40ns on 2.6.32_1-12-0-0, Intel(R) Xeon(R) CPU E5620 @ 2.40GHz
// --------------------------------------------------------------------
inline int64_t gettimeofday_us() {
    timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000L + now.tv_usec;
}

inline int64_t gettimeofday_ms() {
    return gettimeofday_us() / 1000L;
}

inline int64_t gettimeofday_s() {
    return gettimeofday_us() / 1000000L;
}

// ----------------------------------------
// Control frequency of operations.
// ----------------------------------------
// Example:
//   EveryManyUS every_1s(1000000L);
//   while (1) {
//       ...
//       if (every_1s) {
//           // be here at most once per second
//       }
//   }
//   
// 控制操作频率类（定时器，微妙单位）
class EveryManyUS {
public:
    explicit EveryManyUS(int64_t interval_us)
        : _last_time_us(cpuwide_time_us())
        , _interval_us(interval_us) {}
    
    operator bool() {
        const int64_t now_us = cpuwide_time_us();
        if (now_us < _last_time_us + _interval_us) {
            return false;
        }
        _last_time_us = now_us;
        return true;
    }

private:
    // 最新频率到期的挂钟时间
    int64_t _last_time_us;
    // 频率，微妙单位
    const int64_t _interval_us;
};

// ---------------
//  Count elapses
// ---------------
// 
// 时间计数器
class Timer {
public:

    enum TimerType {
        STARTED,
    };

    Timer() : _stop(0), _start(0) {}
    explicit Timer(const TimerType) {
        start();
    }

    // Start this timer
    void start() {
        _start = cpuwide_time_ns();
        _stop = _start;
    }
    
    // Stop this timer
    void stop() {
        _stop = cpuwide_time_ns();
    }

    // Get the elapse from start() to stop(), in various units.
    int64_t n_elapsed() const { return _stop - _start; }
    int64_t u_elapsed() const { return n_elapsed() / 1000L; }
    int64_t m_elapsed() const { return u_elapsed() / 1000L; }
    int64_t s_elapsed() const { return m_elapsed() / 1000L; }

    double n_elapsed(double) const { return (double)(_stop - _start); }
    double u_elapsed(double) const { return (double)n_elapsed() / 1000.0; }
    double m_elapsed(double) const { return (double)u_elapsed() / 1000.0; }
    double s_elapsed(double) const { return (double)m_elapsed() / 1000.0; }
    
private:
    int64_t _stop;
    int64_t _start;
};

}  // namespace butil

#endif  // BUTIL_BAIDU_TIME_H

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Time represents an absolute point in coordinated universal time (UTC),
// internally represented as microseconds (s/1,000,000) since the Windows epoch
// (1601-01-01 00:00:00 UTC) (See http://crbug.com/14734).  System-dependent
// clock interface routines are defined in time_PLATFORM.cc.
//
// TimeDelta represents a duration of time, internally represented in
// microseconds.
//
// TimeTicks represents an abstract time that is most of the time incrementing
// for use in measuring time durations. It is internally represented in
// microseconds.  It can not be converted to a human-readable time, but is
// guaranteed not to decrease (if the user changes the computer clock,
// Time::Now() may actually decrease or jump).  But note that TimeTicks may
// "stand still", for example if the computer suspended.
//
// These classes are represented as only a 64-bit value, so they can be
// efficiently passed by value.
// 
// Time 表示协调世界时间 (UTC 时间戳) ，内部表示自 Windows 系统时代时间之后的微秒数。
// Windows 使用 1601-01-01 00:00:00 UTC 时代。UNIX 使用 1970-01-01 00:00:00 UTC。
// 系统相关的时间接口例程在 time_PLATFORM.cc 中定义。
// 
// TimeDelta 表示一个时间期限（一个持续时间段）。内部以微秒表示。两个 time 之间的时间差
// 也可以表示为一个 TimeDelta 。
//
// TimeTicks 代表一个抽象时间（依赖于平台的滴答计数，分辨率一般是 〜1-15ms）。大部分时
// 候是增加的，主要用于测量持续时间的期限，它在内部表示为微秒。它不能转换为人类可读的时间。
// 但是保证不会减少（如果用户更改计算机时钟，Time::Now() 可能实际上会减少或跳变）。但请
// 注意，TimeTicks 可能会 "stand still" “静止”， 例如，如果计算机阻塞暂停。
//
//
// 这些类仅表示为 64 位值，因此可以通过值有效传递。


// @tips
// 整个地球分为二十四时区，每个时区都有自己的本地时间。在国际上，为了统一起见，我们使用一个统一
// 的时间，称为通用协调时(UTC, Universal Time Coordinated)。UTC 与格林尼治平均时(GMT, 
// Greenwich Mean Time)几乎一样，都与英国伦敦的本地时相同。
// GMT 是比较古老的时间较量标准，根据地球公转自转计算时间。UTC 则是根据原子钟来计算时间，现在基
// 本都用 UTC 时间。
// 
// UNIX 系统时间函数都是基于 1970/1/1 00:00:00 年的，但是有一点值得注意：localtime() 函数返
// 回的 struct tm 结构体成员的 tm_year 为当前年份 -1900 。
// 1900 的资料很少，据说时因为 1900/1/1 为星期一的缘故，方便计算后面某一天是星期几，距 1900/1/1 
// 的天数再对 7 取余所得即为星期数。
// 
// 夏令时比标准时快一个小时。例如，在夏令时的实施期间，标准时间的上午 10 点就成了夏令时的上午 
// 11 点。夏令时，又称“日光节约时制”或“夏时制”，是一种为节约能源而人为规定地方时间的制度，在这
// 一制度实行期间所采用的统一时间称为“夏令时间”。一般在天亮早的夏季人为将时间提前一小时，可以使
// 人早起早睡，减少照明量，以充分利用光照资源，从而节约照明用电。各个采纳夏令时的国家具体规定不
// 同。目前全世界有近 110 个国家每年要实行夏令时。
// 冬令时是在冬天使用的标准时间。在使用日光节约时制的地区，夏天时钟拨快一小时，冬天再拨回来。这
// 时采用的是标准时间，也就是冬令时。
// 1986 年至 1991 年，中华人民共和国在全国范围实行了六年夏令时，每年从 4 月中旬的第一个星期日 2 
// 时整（北京时间）到 9 月中旬第一个星期日的凌晨 2 时整（北京夏令时）。除 1986 年因是实行夏令时的
// 第一年，从 5 月 4 日开始到 9 月 14 日结束外，其它年份均按规定的时段施行。夏令时实施期间，将时
// 间调快一小时。 1992 年 4 月 5 日后暂停实行。俄罗斯自从 2011/3/27 日开始永久使用夏令时，把时
// 间拨快一小时，不再调回。
// 在实行夏令时间的阶段，一年中有一小时不存在，比如 1987/4/12 00:00:00 在中国的本地时间中是不存
// 在的。
// 
// 1. 时间服务器返回的时间为 1900 距今的秒数，而我们需要借助 UNIX 时间函数转为可读的时间 ，因此
//    需要先把这个时间减去 70 年（2208988800s）。
// 2. 夏令时的开始、结束时间使用的是时区转化后的当地时间，因此时间服务器获取到的 UTC 时间需要转为
//    本地时间，才能进行时间是否在夏令时区间的判断。


#ifndef BUTIL_TIME_TIME_H_
#define BUTIL_TIME_TIME_H_

#include <time.h>

#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/build_config.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
// Avoid Mac system header macro leak.
#undef TYPE_BOOL
#endif

#if defined(OS_POSIX)
#include <unistd.h>
#include <sys/time.h>
#endif

#if defined(OS_WIN)
// For FILETIME in FromFileTime, until it moves to a new converter class.
// See TODO(iyengar) below.
#include <windows.h>
#endif

#include <limits>

#ifdef BAIDU_INTERNAL
// gejun: Force inclusion of ul_def.h and undef conflict macros
#include <ul_def.h>
#undef Uchar
#undef Ushort
#undef Uint
#undef Max
#undef Min
#undef Exchange
#endif // BAIDU_INTERNAL

namespace butil {

class Time;
class TimeTicks;

// TimeDelta ------------------------------------------------------------------

// TimeDelta 表示一个时间期限（一个持续时间段，比如 1s/1min 时间段），内部以微秒表示。
// 两个 Time 之间的的时间差也可以表示为一个 TimeDelta 。
class BUTIL_EXPORT TimeDelta {
 public:
  TimeDelta() : delta_(0) {
  }

  // Converts units of time to TimeDeltas.
  // 
  // 转换整型到 TimeDeltas 对象。将一个期限值转换为 TimeDelta （内部微妙表示）
  static TimeDelta FromDays(int days);
  static TimeDelta FromHours(int hours);
  static TimeDelta FromMinutes(int minutes);
  static TimeDelta FromSeconds(int64_t secs);
  static TimeDelta FromMilliseconds(int64_t ms);
  static TimeDelta FromMicroseconds(int64_t us);
  static TimeDelta FromSecondsD(double secs);
  static TimeDelta FromMillisecondsD(double ms);
  static TimeDelta FromMicrosecondsD(double us);

#if defined(OS_WIN)
  static TimeDelta FromQPCValue(LONGLONG qpc_value);
#endif

  // Converts an integer value representing TimeDelta to a class. This is used
  // when deserializing a |TimeDelta| structure, using a value known to be
  // compatible. It is not provided as a constructor because the integer type
  // may be unclear from the perspective of a caller.
  // 
  // 整型值转换为 TimeDelta 。即，使用 |delta| 微妙构造 |TimeDelta| 结构体。
  static TimeDelta FromInternalValue(int64_t delta) {
    return TimeDelta(delta);
  }

  // Returns the maximum time delta, which should be greater than any reasonable
  // time delta we might compare it to. Adding or subtracting the maximum time
  // delta to a time or another time delta has an undefined result.
  // 
  // 返回最大 TimeDelta ，它应该大于任何合理的 TimeDelta ，我们可能会比较它。添加或减去
  // 最大的 TimeDelta 具有未定义的结果。
  static TimeDelta Max();

  // Returns the internal numeric value of the TimeDelta object. Please don't
  // use this and do arithmetic on it, as it is more error prone than using the
  // provided operators.
  // For serializing, use FromInternalValue to reconstitute.
  // 
  // 返回 TimeDelta 对象的内部数值。请不要使用它并对其进行算术运算，因为它比使用重载的运算
  // 符容易出错。对于序列化，请使用 FromInternalValue 重新构建。
  int64_t ToInternalValue() const {
    return delta_;
  }

  // Returns true if the time delta is the maximum time delta.
  // 
  // 如果 TimeDelta 是最大的 TimeDelta ，则返回 true 
  bool is_max() const {
    return delta_ == std::numeric_limits<int64_t>::max();
  }

#if defined(OS_POSIX)
  // 格式化为 struct timespec （纳秒精度）
  struct timespec ToTimeSpec() const;
#endif

  // Returns the time delta in some unit. The F versions return a floating
  // point value, the "regular" versions return a rounded-down value.
  //
  // InMillisecondsRoundedUp() instead returns an integer that is rounded up
  // to the next full millisecond.
  // 
  // 返回某个单位的 TimeDelta 。F 版本返回一个浮点值，其他 “常规” 版本返回一个向下
  // 舍入的值（截掉小数点后面值）。
  // 
  // InMillisecondsRoundedUp() 返回一个整数，舍入到下一个毫秒。1100us -> 2ms
  int InDays() const;
  int InHours() const;
  int InMinutes() const;
  double InSecondsF() const;
  int64_t InSeconds() const;
  double InMillisecondsF() const;
  int64_t InMilliseconds() const;
  int64_t InMillisecondsRoundedUp() const;
  int64_t InMicroseconds() const;

  TimeDelta& operator=(TimeDelta other) {
    delta_ = other.delta_;
    return *this;
  }

  // Computations with other deltas.
  TimeDelta operator+(TimeDelta other) const {
    return TimeDelta(delta_ + other.delta_);
  }
  TimeDelta operator-(TimeDelta other) const {
    return TimeDelta(delta_ - other.delta_);
  }

  TimeDelta& operator+=(TimeDelta other) {
    delta_ += other.delta_;
    return *this;
  }
  TimeDelta& operator-=(TimeDelta other) {
    delta_ -= other.delta_;
    return *this;
  }
  // 负号重载
  TimeDelta operator-() const {
    return TimeDelta(-delta_);
  }

  // Computations with ints, note that we only allow multiplicative operations
  // with ints, and additive operations with other deltas.
  // 
  // 与 int 值的运算。请注意，只允许使用 int 的乘/除法运算；以及其他 deltas 的加法运算。
  TimeDelta operator*(int64_t a) const {
    return TimeDelta(delta_ * a);
  }
  TimeDelta operator/(int64_t a) const {
    return TimeDelta(delta_ / a);
  }
  TimeDelta& operator*=(int64_t a) {
    delta_ *= a;
    return *this;
  }
  TimeDelta& operator/=(int64_t a) {
    delta_ /= a;
    return *this;
  }
  int64_t operator/(TimeDelta a) const {
    return delta_ / a.delta_;
  }

  // Defined below because it depends on the definition of the other classes.
  // 
  // 加上一个 Time 绝对时间点
  // 加上一个 TimeTicks 抽象时间点
  Time operator+(Time t) const;
  TimeTicks operator+(TimeTicks t) const;

  // Comparison operators.
  // 
  // TimeDelta 间关系运算符
  bool operator==(TimeDelta other) const {
    return delta_ == other.delta_;
  }
  bool operator!=(TimeDelta other) const {
    return delta_ != other.delta_;
  }
  bool operator<(TimeDelta other) const {
    return delta_ < other.delta_;
  }
  bool operator<=(TimeDelta other) const {
    return delta_ <= other.delta_;
  }
  bool operator>(TimeDelta other) const {
    return delta_ > other.delta_;
  }
  bool operator>=(TimeDelta other) const {
    return delta_ >= other.delta_;
  }

 private:
  friend class Time;
  friend class TimeTicks;
  // int64_t 与 TimeDelta 乘法运算符重载函数
  friend TimeDelta operator*(int64_t a, TimeDelta td);

  // Constructs a delta given the duration in microseconds. This is private
  // to avoid confusion by callers with an integer constructor. Use
  // FromSeconds, FromMilliseconds, etc. instead.
  // 
  // 构造给定微妙持续时间的 TimeDelta 。私有方法以避免与整数构造函数的调用者混淆。
  // 使用在 FromSeconds ，FromMilliseconds 等方法构造 TimeDelta 类。
  explicit TimeDelta(int64_t delta_us) : delta_(delta_us) {
  }

  // Delta in microseconds.
  // 
  // 微妙时间期限
  int64_t delta_;
};

// int64_t 与 TimeDelta 乘法运算符重载函数
inline TimeDelta operator*(int64_t a, TimeDelta td) {
  return TimeDelta(a * td.delta_);
}

// Time -----------------------------------------------------------------------

// Represents a wall clock time in UTC.
// 
// UTC 挂钟时间，即绝对时间点（即，类似于 "时间戳" 的概念）。
// 内部透明的实现了从 Unix-like（1970）调整到 Windows（1601）时代的微妙时间。
class BUTIL_EXPORT Time {
 public:
  static const int64_t kMillisecondsPerSecond = 1000;
  static const int64_t kMicrosecondsPerMillisecond = 1000;
  static const int64_t kMicrosecondsPerSecond = kMicrosecondsPerMillisecond *
                                              kMillisecondsPerSecond;
  static const int64_t kMicrosecondsPerMinute = kMicrosecondsPerSecond * 60;
  static const int64_t kMicrosecondsPerHour = kMicrosecondsPerMinute * 60;
  static const int64_t kMicrosecondsPerDay = kMicrosecondsPerHour * 24;
  static const int64_t kMicrosecondsPerWeek = kMicrosecondsPerDay * 7;
  static const int64_t kNanosecondsPerMicrosecond = 1000;
  static const int64_t kNanosecondsPerSecond = kNanosecondsPerMicrosecond *
                                             kMicrosecondsPerSecond;

#if !defined(OS_WIN)
  // On Mac & Linux, this value is the delta from the Windows epoch of 1601 to
  // the Posix delta of 1970. This is used for migrating between the old
  // 1970-based epochs to the new 1601-based ones. It should be removed from
  // this global header and put in the platform-specific ones when we remove the
  // migration code.
  // 
  // 在 Mac 和 Linux 上，这个值是从 1601 年的 Windows 纪元到 1970 年的时间段。主要用于
  // 在基于 1970 年的旧时代到基于新的 1601 时代之间调整值。当我们删除迁移代码时，它应该从
  // 这个全局头中删除并放入特定于平台的头中。
  // 
  // 该值 POSIX 平台上为： 11644473600 * kMicrosecondsPerSecond
  static const int64_t kWindowsEpochDeltaMicroseconds;
#endif

  // Represents an exploded time that can be formatted nicely. This is kind of
  // like the Win32 SYSTEMTIME structure or the Unix "struct tm" with a few
  // additions and changes to prevent errors.
  // 
  // 表示可以格式化的时间。类似 Win32 SYSTEMTIME 结构或 Unix "struct tm" 少许添加
  // 和更改。以防止错误。
  struct BUTIL_EXPORT Exploded {
    int year;          // Four digit year "2007"
    int month;         // 1-based month (values 1 = January, etc.)
                       // 基于 1 开始。即， 1～12 月
    int day_of_week;   // 0-based day of week (0 = Sunday, etc.)
    int day_of_month;  // 1-based day of month (1-31)
    int hour;          // Hour within the current day (0-23)
    int minute;        // Minute within the current hour (0-59)
    int second;        // Second within the current minute (0-59 plus leap
                       //   seconds which may take it up to 60).
    int millisecond;   // Milliseconds within the current second (0-999)

    // A cursory test for whether the data members are within their
    // respective ranges. A 'true' return value does not guarantee the
    // Exploded value can be successfully converted to a Time value.
    // 
    // 对数据成员是否在其各自的范围内的粗略测试。注意返回 true 不能保证 Exploded 值
    // 可以成功转换为 Time 值。
    bool HasValidValues() const;
  };

  // Contains the NULL time. Use Time::Now() to get the current time.
  // 
  // 表示一个 NULL 空时间点。 Time::Now() 获取当前时间点。
  Time() : us_(0) {
  }

  // Returns true if the time object has not been initialized.
  // 
  // 如果时间对象尚未初始化，则返回 true 
  bool is_null() const {
    return us_ == 0;
  }

  // Returns true if the time object is the maximum time.
  // 
  // 如果时间对象是最大时间，则返回 true
  bool is_max() const {
    return us_ == std::numeric_limits<int64_t>::max();
  }

  // Returns the time for epoch in Unix-like system (Jan 1, 1970).
  // 
  // 返回 Unix-like（1970）调整到 Windows（1601）时代时间需要的调整的微妙值。
  // 
  // 该值 POSIX 平台上为： 11644473600 * kMicrosecondsPerSecond
  static Time UnixEpoch();

  // Returns the current time. Watch out, the system might adjust its clock
  // in which case time will actually go backwards. We don't guarantee that
  // times are increasing, or that two calls to Now() won't be the same.
  // 
  // 自时代以来，所“走过”的微秒 64-bit 整型值。函数包含了，从 Unix (1970) 调整到 
  // Windows (1601) 时代的逻辑。
  // 
  // 请注意，系统可能会调整其时钟，在这种情况下，时间可能会跳变。我们不保证时间不断增
  // 加，或者对 Now() 的两个调用不一样。
  static Time Now();

  // Returns the maximum time, which should be greater than any reasonable time
  // with which we might compare it.
  // 
  // 返回最大时间，应该大于我们可能比较的任何合理时间。
  static Time Max();

  // Returns the current time. Same as Now() except that this function always
  // uses system time so that there are no discrepancies between the returned
  // time and system time even on virtual environments including our test bot.
  // For timing sensitive unittests, this function should be used.
  // 
  // 返回当前时间戳。与 Now() 相同，只是该函数始终使用系统时间，即使在包括我们的测试机器
  // 人在内的虚拟环境下，返回的时间和系统时间之间也不会有差异。对于时间敏感单位测试，应使
  // 用此功能。注： POSIX/MAC 都直接调用了 Now() 接口方法。
  static Time NowFromSystemTime();

  // Converts to/from time_t in UTC and a Time class.
  // TODO(brettw) this should be removed once everybody starts using the |Time|
  // class.
  // 
  // 将时间转换为自时代（1970/1/1）以来的秒数的整型值。
  static Time FromTimeT(time_t tt);
  time_t ToTimeT() const;

  // Converts time to/from a double which is the number of seconds since epoch
  // (Jan 1, 1970).  Webkit uses this format to represent time.
  // Because WebKit initializes double time value to 0 to indicate "not
  // initialized", we map it to empty Time object that also means "not
  // initialized".
  // 
  // 将时间转换为自时代（1970/1/1）以来的秒数的浮点数。 Webkit 使用这种格式来表示时间。
  // 由于 WebKit 将双精度初始化为 0 以指示 “未初始化”，因此我们将其映射为空的时间对象，
  // 也意味着“未初始化”。
  static Time FromDoubleT(double dt);
  double ToDoubleT() const;

#if defined(OS_POSIX)
  // Converts the timespec structure to time. MacOS X 10.8.3 (and tentatively,
  // earlier versions) will have the |ts|'s tv_nsec component zeroed out,
  // having a 1 second resolution, which agrees with
  // https://developer.apple.com/legacy/library/#technotes/tn/tn1150.html#HFSPlusDates.
  // 
  // 将 timespec 结构转换为 Time 。 MacOS X 10.8.3（暂时为早期版本）会将 |ts| 的 tv_nsec 组
  // 件清零，具有 1 秒的分辨率。
  static Time FromTimeSpec(const timespec& ts);
#endif

  // Converts to/from the Javascript convention for times, a number of
  // milliseconds since the epoch:
  // https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date/getTime.
  // 
  // 将 Javascript 时间戳转换为 Time 时间。 Javascript 是从时代（1970/1/1 时代）开始数毫秒数。
  static Time FromJsTime(double ms_since_epoch);
  double ToJsTime() const;

  // Converts to Java convention for times, a number of
  // milliseconds since the epoch.
  // 
  // 转换为 Java 约定的时间。 Java 是从时代（1970/1/1 时代）开始的毫秒数。
  int64_t ToJavaTime() const;

#if defined(OS_POSIX)
  static Time FromTimeVal(struct timeval t);
  struct timeval ToTimeVal() const;
#endif

#if defined(OS_MACOSX)
  static Time FromCFAbsoluteTime(CFAbsoluteTime t);
  CFAbsoluteTime ToCFAbsoluteTime() const;
#endif

#if defined(OS_WIN)
  static Time FromFileTime(FILETIME ft);
  FILETIME ToFileTime() const;

  // The minimum time of a low resolution timer.  This is basically a windows
  // constant of ~15.6ms.  While it does vary on some older OS versions, we'll
  // treat it as static across all windows versions.
  // 
  // 低分辨定时器的最短时间。这基本上是 ~15.6ms 的 windows 常量。虽然它在某些旧的操作
  // 系统版本上有所不同，但我们会在所有 Windows 版本中将其视为静态。
  static const int kMinLowResolutionThresholdMs = 16;

  // Enable or disable Windows high resolution timer. If the high resolution
  // timer is not enabled, calls to ActivateHighResolutionTimer will fail.
  // When disabling the high resolution timer, this function will not cause
  // the high resolution timer to be deactivated, but will prevent future
  // activations.
  // Must be called from the main thread.
  // For more details see comments in time_win.cc.
  // 
  // 启用或禁用 Windows 高分辨率计时器。如果高分辨率定时器未启用，则对 
  // ActivateHighResolutionTimer 的调用将失败。禁用高分辨率定时器时，该功能不会导致
  // 高分辨率定时器停用，但会阻止将来激活。必须从主线程调用。
  static void EnableHighResolutionTimer(bool enable);

  // Activates or deactivates the high resolution timer based on the |activate|
  // flag.  If the HighResolutionTimer is not Enabled (see
  // EnableHighResolutionTimer), this function will return false.  Otherwise
  // returns true.  Each successful activate call must be paired with a
  // subsequent deactivate call.
  // All callers to activate the high resolution timer must eventually call
  // this function to deactivate the high resolution timer.
  // 
  // 根据 |activate| 激活或关闭高分辨率定时器。如果 HighResolutionTimer 未启用
  // （请参阅 EnableHighResolutionTimer ），则此函数将返回 false 。否则返回 true 。
  // 每个成功的激活调用必须与随后的去激活调用配对。所有调用者激活高分辨率定时器必须
  // 最终调用此功能来关闭高分辨率定时器。
  static bool ActivateHighResolutionTimer(bool activate);

  // Returns true if the high resolution timer is both enabled and activated.
  // This is provided for testing only, and is not tracked in a thread-safe
  // way.
  // 
  // 如果高分辨率定时器既启用且激活，则返回 true 。这仅用于测试，不能以线程安全的方式进
  // 行跟踪。
  static bool IsHighResolutionTimerInUse();
#endif

  // Converts an exploded structure representing either the local time or UTC
  // into a Time class.
  // 
  // 将代表 local 时间或 UTC 时间 Exploded 的结构转换为 Time 类。
  static Time FromUTCExploded(const Exploded& exploded) {
    return FromExploded(false, exploded);
  }
  static Time FromLocalExploded(const Exploded& exploded) {
    return FromExploded(true, exploded);
  }

  // Converts an integer value representing Time to a class. This is used
  // when deserializing a |Time| structure, using a value known to be
  // compatible. It is not provided as a constructor because the integer type
  // may be unclear from the perspective of a caller.
  // 
  // 将表示 Time 的整数值转换为类。这在反序列化 |Time| 时使用，使用已知的兼容值。它不是
  // 作为构造函数提供的，因为从调用者的角度来看整数类型可能不清楚。
  static Time FromInternalValue(int64_t us) {
    return Time(us);
  }

  // Converts a string representation of time to a Time object.
  // An example of a time string which is converted is as below:-
  // "Tue, 15 Nov 1994 12:45:26 GMT". If the timezone is not specified
  // in the input string, FromString assumes local time and FromUTCString
  // assumes UTC. A timezone that cannot be parsed (e.g. "UTC" which is not
  // specified in RFC822) is treated as if the timezone is not specified.
  // TODO(iyengar) Move the FromString/FromTimeT/ToTimeT/FromFileTime to
  // a new time converter class.
  // 
  // 将时间的字符串表示形式转换为 Time 对象。转换的时间字符串示例如下：- 
  // "Tue, 15 Nov 1994 12:45:26 GMT" 。如果未在输入字符串中指定时区，则 FromString 假
  // 定为本地时间， FromUTCString 假定为 UTC 。无法解析的时区（例如， RFC822 中未指定的 
  // "UTC" ）被视为未指定时区。
  static bool FromString(const char* time_string, Time* parsed_time) {
    return FromStringInternal(time_string, true, parsed_time);
  }
  static bool FromUTCString(const char* time_string, Time* parsed_time) {
    return FromStringInternal(time_string, false, parsed_time);
  }

  // For serializing, use FromInternalValue to reconstitute. Please don't use
  // this and do arithmetic on it, as it is more error prone than using the
  // provided operators.
  // 
  // 对于序列化，请使用 FromInternalValue 进行重组。请不要使用它并对它进行算术运算，因为
  // 它比使用提供的运算符更容易出错。
  int64_t ToInternalValue() const {
    return us_;
  }

  // Fills the given exploded structure with either the local time or UTC from
  // this time structure (containing UTC).
  // 
  // 使用 local 时间或 UTC 时间结构（包含 UTC ）填充给定的结构。
  void UTCExplode(Exploded* exploded) const {
    return Explode(false, exploded);
  }
  void LocalExplode(Exploded* exploded) const {
    return Explode(true, exploded);
  }

  // Rounds this time down to the nearest day in local time. It will represent
  // midnight on that day.
  // 
  // 当地时间的最近一天（即，它代表当天午夜 00:00:00 点）
  Time LocalMidnight() const;

  Time& operator=(Time other) {
    us_ = other.us_;
    return *this;
  }

  // Compute the difference between two times.
  // 
  // 计算时间差值。
  TimeDelta operator-(Time other) const {
    return TimeDelta(us_ - other.us_);
  }

  // Modify by some time delta.
  // 
  // 添加/减少一个持续时间。
  Time& operator+=(TimeDelta delta) {
    us_ += delta.delta_;
    return *this;
  }
  Time& operator-=(TimeDelta delta) {
    us_ -= delta.delta_;
    return *this;
  }

  // Return a new time modified by some delta.
  // 
  // 返回一个 添加/减少一个持续时间 的一个时间结构体
  Time operator+(TimeDelta delta) const {
    return Time(us_ + delta.delta_);
  }
  Time operator-(TimeDelta delta) const {
    return Time(us_ - delta.delta_);
  }

  // Comparison operators
  // 
  // 关系运算符重载
  bool operator==(Time other) const {
    return us_ == other.us_;
  }
  bool operator!=(Time other) const {
    return us_ != other.us_;
  }
  bool operator<(Time other) const {
    return us_ < other.us_;
  }
  bool operator<=(Time other) const {
    return us_ <= other.us_;
  }
  bool operator>(Time other) const {
    return us_ > other.us_;
  }
  bool operator>=(Time other) const {
    return us_ >= other.us_;
  }

 private:
  friend class TimeDelta;

  // 私有的整型到时间结构的构造函数
  explicit Time(int64_t us) : us_(us) {
  }

  // Explodes the given time to either local time |is_local = true| or UTC
  // |is_local = false|.
  // 
  // 将给定时间分解为本地时间或 UTC 时间 (|is_local=true| or |is_local=false|)
  void Explode(bool is_local, Exploded* exploded) const;

  // Unexplodes a given time assuming the source is either local time
  // |is_local = true| or UTC |is_local = false|.
  // 
  // 将代表 local 时间或 UTC 时间 Exploded 的结构转换为 Time 类。
  static Time FromExploded(bool is_local, const Exploded& exploded);

  // Converts a string representation of time to a Time object.
  // An example of a time string which is converted is as below:-
  // "Tue, 15 Nov 1994 12:45:26 GMT". If the timezone is not specified
  // in the input string, local time |is_local = true| or
  // UTC |is_local = false| is assumed. A timezone that cannot be parsed
  // (e.g. "UTC" which is not specified in RFC822) is treated as if the
  // timezone is not specified.
  // 
  // 将时间的字符串表示形式转换为 Time 对象。转换的时间字符串示例如下：- "Tue, 
  // 15 Nov 1994 12:45:26 GMT" 。如果未在输入字符串中指定时区，则本地时间 
  // |is_local=true| 或 UTC |is_local=false | 。无法分析的时区（例如，RFC822 
  // 中未指定的 “UTC” ）被视为未指定时区。
  static bool FromStringInternal(const char* time_string,
                                 bool is_local,
                                 Time* parsed_time);

  // The representation of Jan 1, 1970 UTC in microseconds since the
  // platform-dependent epoch.
  // 
  // 1970/1/1 UTC 以微秒表示的平台相关时代以来的表示。
  static const int64_t kTimeTToMicrosecondsOffset;

#if defined(OS_WIN)
  // Indicates whether fast timers are usable right now.  For instance,
  // when using battery power, we might elect to prevent high speed timers
  // which would draw more power.
  static bool high_resolution_timer_enabled_;
  // Count of activations on the high resolution timer.  Only use in tests
  // which are single threaded.
  static int high_resolution_timer_activated_;
#endif

  // Time in microseconds in UTC.
  // 
  // 以微秒为单位 UTC 时间戳。内部透明的实现了从 Unix-like(1970) 调整到 Windows
  // (1601)时代的微妙时间。
  int64_t us_;
};

// Inline the TimeDelta factory methods, for fast TimeDelta construction.
// 
// 内联 TimeDelta 工厂方法，实现快速 TimeDelta 构造。

// static
inline TimeDelta TimeDelta::FromDays(int days) {
  // Preserve max to prevent overflow.
  if (days == std::numeric_limits<int>::max())
    return Max();
  return TimeDelta(days * Time::kMicrosecondsPerDay);
}

// static
inline TimeDelta TimeDelta::FromHours(int hours) {
  // Preserve max to prevent overflow.
  if (hours == std::numeric_limits<int>::max())
    return Max();
  return TimeDelta(hours * Time::kMicrosecondsPerHour);
}

// static
inline TimeDelta TimeDelta::FromMinutes(int minutes) {
  // Preserve max to prevent overflow.
  if (minutes == std::numeric_limits<int>::max())
    return Max();
  return TimeDelta(minutes * Time::kMicrosecondsPerMinute);
}

// static
inline TimeDelta TimeDelta::FromSeconds(int64_t secs) {
  // Preserve max to prevent overflow.
  if (secs == std::numeric_limits<int64_t>::max())
    return Max();
  return TimeDelta(secs * Time::kMicrosecondsPerSecond);
}

// static
inline TimeDelta TimeDelta::FromMilliseconds(int64_t ms) {
  // Preserve max to prevent overflow.
  if (ms == std::numeric_limits<int64_t>::max())
    return Max();
  return TimeDelta(ms * Time::kMicrosecondsPerMillisecond);
}

// 双精度版本

// static
inline TimeDelta TimeDelta::FromSecondsD(double secs) {
  // Preserve max to prevent overflow.
  if (secs == std::numeric_limits<double>::infinity())
    return Max();
  return TimeDelta((int64_t)(secs * Time::kMicrosecondsPerSecond));
}

// static
inline TimeDelta TimeDelta::FromMillisecondsD(double ms) {
  // Preserve max to prevent overflow.
  if (ms == std::numeric_limits<double>::infinity())
    return Max();
  return TimeDelta((int64_t)(ms * Time::kMicrosecondsPerMillisecond));
}

// static
inline TimeDelta TimeDelta::FromMicroseconds(int64_t us) {
  // Preserve max to prevent overflow.
  if (us == std::numeric_limits<int64_t>::max())
    return Max();
  return TimeDelta(us);
}

// static
inline TimeDelta TimeDelta::FromMicrosecondsD(double us) {
  // Preserve max to prevent overflow.
  if (us == std::numeric_limits<double>::infinity())
    return Max();
  return TimeDelta((int64_t)us);
}

// 绝对时间点加上一个时间期限
inline Time TimeDelta::operator+(Time t) const {
  return Time(t.us_ + delta_);
}

// TimeTicks ------------------------------------------------------------------

// TimeTicks 代表一个抽象时间（依赖于平台的滴答计数，分辨率一般是 〜1-15ms）
class BUTIL_EXPORT TimeTicks {
 public:
  // We define this even without OS_CHROMEOS for seccomp sandbox testing.
#if defined(OS_LINUX)
  // Force definition of the system trace clock; it is a chromeos-only api
  // at the moment and surfacing it in the right place requires mucking
  // with glibc et al.
  // 
  // 强制定义系统跟踪时钟; 这是一个 chromeos 唯一的 api ，并在正确的地方浮出水面，
  // 需要与 glibc 等共享。
  static const clockid_t kClockSystemTrace = 11;
#endif

  TimeTicks() : ticks_(0) {
  }

  // Platform-dependent tick count representing "right now."
  // The resolution of this clock is ~1-15ms.  Resolution varies depending
  // on hardware/operating system configuration.
  // 
  // 依赖于平台的滴答计数表示“现在”。这个时钟的分辨率是 〜1-15ms 。分辨率因硬件/操作
  // 系统配置而异。
  static TimeTicks Now();

  // Returns a platform-dependent high-resolution tick count. Implementation
  // is hardware dependent and may or may not return sub-millisecond
  // resolution.  THIS CALL IS GENERALLY MUCH MORE EXPENSIVE THAN Now() AND
  // SHOULD ONLY BE USED WHEN IT IS REALLY NEEDED.
  // 
  // 返回平台相关的高分辨率滴答计数。实现取决于硬件，可能会或可能不会返回亚毫秒级的分辨率。
  // 这个调用通常比 Now() 更加昂贵，并且只有在它真的需要时才能使用。
  static TimeTicks HighResNow();

  static bool IsHighResNowFastAndReliable();

  // FIXME(gejun): CLOCK_THREAD_CPUTIME_ID behaves differently in different
  // glibc. To avoid potential bug, disable ThreadNow() always.
  // Returns true if ThreadNow() is supported on this system.
  // 
  // FIXME（gejun）：CLOCK_THREAD_CPUTIME_ID 在不同的 glibc 中表现不同。为避免潜在
  // 的错误，请始终禁用 ThreadNow() 。如果此系统支持 ThreadNow() ，则返回 true
  static bool IsThreadNowSupported() {
      return false;
  }

  // Returns thread-specific CPU-time on systems that support this feature.
  // Needs to be guarded with a call to IsThreadNowSupported(). Use this timer
  // to (approximately) measure how much time the calling thread spent doing
  // actual work vs. being de-scheduled. May return bogus results if the thread
  // migrates to another CPU between two calls.
  // 
  // 在支持此功能的系统上返回线程特定的 CPU 时间。需要通过调用 IsThreadNowSupported() 来
  // 保护。使用这个计时器来（近似）测量调用线程花费多少时间做实际工作与被解除调度。如果线程在
  // 两次调用之间迁移到另一个 CPU ，可能会返回虚假结果。
  static TimeTicks ThreadNow();

  // Returns the current system trace time or, if none is defined, the current
  // high-res time (i.e. HighResNow()). On systems where a global trace clock
  // is defined, timestamping TraceEvents's with this value guarantees
  // synchronization between events collected inside chrome and events
  // collected outside (e.g. kernel, X server).
  // 
  // 返回当前系统跟踪时间，如果没有定义，则返回当前高分辨率时间（即 HighResNow() ）。在定
  // 义了全局跟踪时钟的系统上，使用此值对 TraceEvents 进行时间戳记可确保 chrome 内收集的
  // 事件与外部收集的事件（例如内核，X 服务器）之间的同步。
  static TimeTicks NowFromSystemTraceTime();

#if defined(OS_WIN)
  // Get the absolute value of QPC time drift. For testing.
  // 
  // 获取 QPC 时间漂移的绝对值。供测试用。
  static int64_t GetQPCDriftMicroseconds();

  static TimeTicks FromQPCValue(LONGLONG qpc_value);

  // Returns true if the high resolution clock is working on this system.
  // This is only for testing.
  static bool IsHighResClockWorking();

  // Enable high resolution time for TimeTicks::Now(). This function will
  // test for the availability of a working implementation of
  // QueryPerformanceCounter(). If one is not available, this function does
  // nothing and the resolution of Now() remains 1ms. Otherwise, all future
  // calls to TimeTicks::Now() will have the higher resolution provided by QPC.
  // Returns true if high resolution time was successfully enabled.
  static bool SetNowIsHighResNowIfSupported();

  // Returns a time value that is NOT rollover protected.
  static TimeTicks UnprotectedNow();
#endif

  // Returns true if this object has not been initialized.
  bool is_null() const {
    return ticks_ == 0;
  }

  // Converts an integer value representing TimeTicks to a class. This is used
  // when deserializing a |TimeTicks| structure, using a value known to be
  // compatible. It is not provided as a constructor because the integer type
  // may be unclear from the perspective of a caller.
  // 
  // 将表示 TimeTicks 的整数值转换为类。这在反序列化 |TimeTicks| 时使用，使用已知兼容的值。
  // 它不是作为构造函数提供的，因为从调用者的角度来看，整数类型可能被意外转换。
  static TimeTicks FromInternalValue(int64_t ticks) {
    return TimeTicks(ticks);
  }

  // Get the TimeTick value at the time of the UnixEpoch. This is useful when
  // you need to relate the value of TimeTicks to a real time and date.
  // Note: Upon first invocation, this function takes a snapshot of the realtime
  // clock to establish a reference point.  This function will return the same
  // value for the duration of the application, but will be different in future
  // application runs.
  // 
  // 获取 UnixEpoch 时的 TimeTick 值。当您需要将 TimeTicks 的值与实时和日期相关联时，这
  // 非常有用。
  // 注意：首次调用时，此函数会获取实时时钟的快照以建立参考点。此函数将在应用程序的持续时间内
  // 返回相同的值，但在将来的应用程序运行中将会有所不同。
  static TimeTicks UnixEpoch();

  // Returns the internal numeric value of the TimeTicks object.
  // For serializing, use FromInternalValue to reconstitute.
  // 
  // 返回 TimeTicks 对象的内部数值。对于序列化，请使用 FromInternalValue 重新构建。
  int64_t ToInternalValue() const {
    return ticks_;
  }

  TimeTicks& operator=(TimeTicks other) {
    ticks_ = other.ticks_;
    return *this;
  }

  // Compute the difference between two times.
  TimeDelta operator-(TimeTicks other) const {
    return TimeDelta(ticks_ - other.ticks_);
  }

  // Modify by some time delta.
  TimeTicks& operator+=(TimeDelta delta) {
    ticks_ += delta.delta_;
    return *this;
  }
  TimeTicks& operator-=(TimeDelta delta) {
    ticks_ -= delta.delta_;
    return *this;
  }

  // Return a new TimeTicks modified by some delta.
  TimeTicks operator+(TimeDelta delta) const {
    return TimeTicks(ticks_ + delta.delta_);
  }
  TimeTicks operator-(TimeDelta delta) const {
    return TimeTicks(ticks_ - delta.delta_);
  }

  // Comparison operators
  bool operator==(TimeTicks other) const {
    return ticks_ == other.ticks_;
  }
  bool operator!=(TimeTicks other) const {
    return ticks_ != other.ticks_;
  }
  bool operator<(TimeTicks other) const {
    return ticks_ < other.ticks_;
  }
  bool operator<=(TimeTicks other) const {
    return ticks_ <= other.ticks_;
  }
  bool operator>(TimeTicks other) const {
    return ticks_ > other.ticks_;
  }
  bool operator>=(TimeTicks other) const {
    return ticks_ >= other.ticks_;
  }

 protected:
  friend class TimeDelta;

  // Please use Now() to create a new object. This is for internal use
  // and testing. Ticks is in microseconds.
  explicit TimeTicks(int64_t ticks) : ticks_(ticks) {
  }

  // Tick count in microseconds.
  int64_t ticks_;

#if defined(OS_WIN)
  typedef DWORD (*TickFunctionType)(void);
  static TickFunctionType SetMockTickFunction(TickFunctionType ticker);
#endif
};

inline TimeTicks TimeDelta::operator+(TimeTicks t) const {
  return TimeTicks(t.ticks_ + delta_);
}

}  // namespace butil

#endif  // BUTIL_TIME_TIME_H_

// Copyright (c) 2012 Baidu, Inc.
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
// Date: 2012-10-08 23:53:50

// Merged chromium log and streaming log.

#ifndef BUTIL_LOGGING_H_
#define BUTIL_LOGGING_H_

// @todo
#include <butil/config.h>   // BRPC_WITH_GLOG

#include <inttypes.h>
#include <string>
#include <cstring>
#include <sstream>
#include "butil/macros.h"    // BAIDU_CONCAT
#include "butil/atomicops.h" // Used by LOG_EVERY_N, LOG_FIRST_N etc
#include "butil/time.h"      // gettimeofday_us()

#if BRPC_WITH_GLOG
# include <glog/logging.h>
# include <glog/raw_logging.h>
// define macros that not implemented in glog
# ifndef DCHECK_IS_ON   // glog didn't define DCHECK_IS_ON in older version
#  if defined(NDEBUG)
#    define DCHECK_IS_ON() 0
#  else
#    define DCHECK_IS_ON() 1
#  endif  // NDEBUG
# endif // DCHECK_IS_ON
# if DCHECK_IS_ON() 
#  define DPLOG(...) PLOG(__VA_ARGS__)
#  define DPLOG_IF(...) PLOG_IF(__VA_ARGS__)
#  define DPCHECK(...) PCHECK(__VA_ARGS__)
#  define DVPLOG(...) VLOG(__VA_ARGS__)
# else 
#  define DPLOG(...) DLOG(__VA_ARGS__)
#  define DPLOG_IF(...) DLOG_IF(__VA_ARGS__)
#  define DPCHECK(...) DCHECK(__VA_ARGS__)
#  define DVPLOG(...) DVLOG(__VA_ARGS__)
# endif

#define LOG_AT(severity, file, line)                                    \
    google::LogMessage(file, line, google::severity).stream()

#else

#ifdef BAIDU_INTERNAL
// gejun: com_log.h includes ul_def.h, undef conflict macros
// FIXME(gejun): We have to include com_log which is assumed to be included
// in other modules right now.
#include <com_log.h>
#undef Uchar
#undef Ushort
#undef Uint
#undef Max
#undef Min
#undef Exchange
#endif // BAIDU_INTERNAL

// @tips
// See https://github.com/gflags/gflags
// gflags 是 Google 开源的一个命令行 flag （区别于参数）库。和 getopt() 之类的库不同，
// flag 的定义可以散布在各个源码中，而不用放在一起。一个源码文件可以定义一些它自己的 flag，
// 链接了该文件的应用都能使用这些 flag 。这样就能非常方便地复用代码。如果不同的文件定义了
// 相同的 flag ，链接时会报错。
// 
// Use like:
// 
// #include <gflags/gflags.h>
// #include <iostream>
// 
// DEFINE_bool(btest, true, "btest options");
// DEFINE_string(stest, "ss", "stest options");
// 
// int main(int argc, char** argv) {
//      gflags::SetUsageMessage("there is two param"
//      "Usage:\n"
//      "./a.out [FLAGS]\n");
//  
//      std::cout << FLAGS_btest << FLAGS_stest << std::endl;
//  
//      // Google flags.
//      int* pargc = &argc;
//      char*** pargv = &argv;
//      // true 表示不保留定义的 flags
//      ::gflags::ParseCommandLineFlags(pargc, pargv, true);
//      gflags::ShowUsageWithFlagsRestrict(argv[0], " this is gflags useage test");
//      
//      return 0;
// }
// 
// 二. flag 还可以实现一处定义多处访问的功能。flag 支持的类型有：
// DEFINE_bool: boolean
// DEFINE_int32: 32-bit integer
// DEFINE_int64: 64-bit integer
// DEFINE_uint64: unsigned 64-bit integer
// DEFINE_double: double
// DEFINE_string: C++ string
// 
// Use like:
// \file Healer.h
// #include <gflags/gflags.h>
// DEFINE_bool(test, true, "This text should be stripped out");
// 
// \file main.cpp
// #include "Header.h"
// #include <gflags/gflags_declare.h>
// #include <iostream>
// 
// DECLARE_bool(test); // in head.h
// int main(int argc, char** argv) {
//     std::cout <<  FLAGS_test << std::endl;
//     system("pause");
//     return 0;
// }


#include <inttypes.h>
#include <gflags/gflags_declare.h>

#include "butil/base_export.h"
#include "butil/basictypes.h"
#include "butil/debug/debugger.h"
#include "butil/strings/string_piece.h"
#include "butil/build_config.h"
#include "butil/synchronization/lock.h"
//
// Optional message capabilities
// -----------------------------
// Assertion failed messages and fatal errors are displayed in a dialog box
// before the application exits. However, running this UI creates a message
// loop, which causes application messages to be processed and potentially
// dispatched to existing application windows. Since the application is in a
// bad state when this assertion dialog is displayed, these messages may not
// get processed and hang the dialog, or the application might go crazy.
//
// Therefore, it can be beneficial to display the error dialog in a separate
// process from the main application. When the logging system needs to display
// a fatal error dialog box, it will look for a program called
// "DebugMessage.exe" in the same directory as the application executable. It
// will run this application with the message as the command line, and will
// not include the name of the application as is traditional for easier
// parsing.
//
// The code for DebugMessage.exe is only one line. In WinMain, do:
//   MessageBox(NULL, GetCommandLineW(), L"Fatal Error", 0);
//
// If DebugMessage.exe is not found, the logging code will use a normal
// MessageBox, potentially causing the problems discussed above.
// 
// 
// 可选的消息功能
// -----------------------------
// 在应用程序退出之前，断言失败的消息和致命错误将显示在对话框中。但是，运行此用户界面会创
// 建一个消息循环，这会导致应用程序消息被处理并可能被分派到现有的已存在应用程序窗口。由于
// 当这个断言对话框被显示时，应用程序就会处于不良状态，因此这些消息可能无法处理并挂起对话
// 框，或者应用程序可能 "发疯"。
// 
// 因此，在与主应用程序分开的进程中显示错误对话框可能是有益的。当日志系统需要显示一个致命
// 的错误对话框时，它会在与应用程序可执行文件相同的目录中查找名为 "DebugMessage.exe" 的
// 程序。它将以消息作为命令行运行此应用程序，并且不会像传统的那样包含应用程序的名称，以便
// 于解析。
// 
// 如果找不到 DebugMessage.exe ，则日志记录代码将使用正常的 MessageBox ，这可能会导致
// 上述问题（应用程序 "发疯" ）。


// Instructions
// ------------
//
// Make a bunch of macros for logging.  The way to log things is to stream
// things to LOG(<a particular severity level>).  E.g.,
//
//   LOG(INFO) << "Found " << num_cookies << " cookies";
//
// You can also do conditional logging:
//
//   LOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// The CHECK(condition) macro is active in both debug and release builds and
// effectively performs a LOG(FATAL) which terminates the process and
// generates a crashdump unless a debugger is attached.
//
// There are also "debug mode" logging macros like the ones above:
//
//   DLOG(INFO) << "Found cookies";
//
//   DLOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// All "debug mode" logging is compiled away to nothing for non-debug mode
// compiles.  LOG_IF and development flags also work well together
// because the code can be compiled away sometimes.
//
// We also have
//
//   LOG_ASSERT(assertion);
//   DLOG_ASSERT(assertion);
//
// which is syntactic sugar for {,D}LOG_IF(FATAL, assert fails) << assertion;
//
// There are "verbose level" logging macros.  They look like
//
//   VLOG(1) << "I'm printed when you run the program with --v=1 or more";
//   VLOG(2) << "I'm printed when you run the program with --v=2 or more";
//
// These always log at the INFO log level (when they log at all).
// The verbose logging can also be turned on module-by-module.  For instance,
//    --vmodule=profile=2,icon_loader=1,browser_*=3,*/chromeos/*=4 --v=0
// will cause:
//   a. VLOG(2) and lower messages to be printed from profile.{h,cc}
//   b. VLOG(1) and lower messages to be printed from icon_loader.{h,cc}
//   c. VLOG(3) and lower messages to be printed from files prefixed with
//      "browser"
//   d. VLOG(4) and lower messages to be printed from files under a
//     "chromeos" directory.
//   e. VLOG(0) and lower messages to be printed from elsewhere
//
// The wildcarding functionality shown by (c) supports both '*' (match
// 0 or more characters) and '?' (match any single character)
// wildcards.  Any pattern containing a forward or backward slash will
// be tested against the whole pathname and not just the module.
// E.g., "*/foo/bar/*=2" would change the logging level for all code
// in source files under a "foo/bar" directory.
//
// There's also VLOG_IS_ON(n) "verbose level" condition macro. To be used as
//
//   if (VLOG_IS_ON(2)) {
//     // do some logging preparation and logging
//     // that can't be accomplished with just VLOG(2) << ...;
//   }
//
// There is also a VLOG_IF "verbose level" condition macro for sample
// cases, when some extra computation and preparation for logs is not
// needed.
//
//   VLOG_IF(1, (size > 1024))
//      << "I'm printed when size is more than 1024 and when you run the "
//         "program with --v=1 or more";
//
// Lastly, there is:
//
//   PLOG(ERROR) << "Couldn't do foo";
//   DPLOG(ERROR) << "Couldn't do foo";
//   PLOG_IF(ERROR, cond) << "Couldn't do foo";
//   DPLOG_IF(ERROR, cond) << "Couldn't do foo";
//   PCHECK(condition) << "Couldn't do foo";
//   DPCHECK(condition) << "Couldn't do foo";
//
// which append the last system error to the message in string form (taken from
// GetLastError() on Windows and errno on POSIX).
//
// The supported severity levels for macros that allow you to specify one
// are (in increasing order of severity) INFO, WARNING, ERROR, and FATAL.
//
// Very important: logging a message at the FATAL severity level causes
// the program to terminate (after the message is logged).
//
// There is the special severity of DFATAL, which logs FATAL in debug mode,
// ERROR in normal mode.
// 
// 1. 制作一堆用于记录的宏。记录事物的方式是流式传输：
// Use like:
//  LOG(INFO) << "Found " << num_cookies << " cookies";
// 
// 2. 你也可以按条件做记录：
// Use like:
//  LOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
// 
// 3. CHECK(condition) 宏在调试版本和发布版本中都处于活动状态。有效地执行 LOG(FATAL) ，
// 终止进程并生成 crashdump ，除非连接了调试器。
// 
// 4. 还有像上面那样的“调试模式”日志记录宏：
// Use like:
//  DLOG(INFO) << "Found cookies";
//  DLOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
// 
// 4. 对于非调试模式编译，所有 "debug mode" 日志都将编译为无。 LOG_IF 和开发标志也可
// 以很好地结合在一起，因为代码有时可以被编译掉。
// 
// 5. 我们也可以使用断言： 
//  LOG_ASSERT(assertion);
//  DLOG_ASSERT(assertion);
// 注：以上两个是 {,D}LOG_IF(FATAL, assert fails) << assertion; 的语法糖
// 
// 6. 有详细记录宏（ "verbose level" 级别）：
// Use like:
//  VLOG(1) << "I'm printed when you run the program with --v=1 or more";
//  VLOG(2) << "I'm printed when you run the program with --v=2 or more";
// 注：这些总是记录 INFO 日志级别的日志。详细日志记录也可以逐个模块地打开。例如：
// --vmodule=profile=2,icon_loader=1,browser_*=3,*/chromeos/*=4 --v=0
// 会产生：
// 
// (c) 所示的通配符功能支持 '*'（匹配 0 或更多字符）和 '?'（匹配任何单个字符）通配符。
// 包含向前或向后斜线的任何模式都将针对整个路径名而不仅仅是模块进行测试。
// 例如，"*/foo/bar/*=2" 会改变 "foo/bar" 目录下源文件中所有代码的日志记录级别。
// 
// 7. 还有 VLOG_IS_ON(n) "verbose level"(详细级别)条件宏。
// Use like:
//  if (VLOG_IS_ON(2)) {
//      // do some logging preparation and logging
//      // that can't be accomplished with just VLOG(2) << ...;
//  }
// 
// 8. 还有一个 VLOG_IF "verbose level"(详细级别) 条件宏。当不需要额外的计算和日志准备
// 时可直接使用。
// Use lie:
//  VLOG_IF(1, (size > 1024))
//  << "I'm printed when size is more than 1024 and when you run the "
//  "program with --v=1 or more";
// 
// 9. 最后，以字符串形式将最后的系统错误追加到消息中（取自 Windows 上的 GetLastError() 
// 和 POSIX 上的 errno ）。允许您指定一个宏的受支持的错误级别（按严重性的升序排列）INFO,
// WARNING,ERROR,FATAL.
// Use like:
//  PLOG(ERROR) << "Couldn't do foo";
//  DPLOG(ERROR) << "Couldn't do foo";
//  PLOG_IF(ERROR, cond) << "Couldn't do foo";
//  DPLOG_IF(ERROR, cond) << "Couldn't do foo";
//  PCHECK(condition) << "Couldn't do foo";
//  DPCHECK(condition) << "Couldn't do foo";
// 注：非常重要：以 FATAL 级别记录消息会导致程序终止（消息记录后）。有 DFATAL 的级别，它
// 在调试模式下记录 FATAL ，在正常模式下记录 ERROR
// 
// CHECK(condition) 宏在调试和发布版本中均处于活动状态，并有效执行一个 LOG(FATAL)， 该
// 命令会终止进程并生成 crashdump，除非连接调试器

namespace logging {

// TODO(avi): do we want to do a unification of character types here?
// 
// 进行字符类型的统一定义
#if defined(OS_WIN)
typedef wchar_t PathChar;
#else
typedef char PathChar;
#endif

// Where to record logging output? A flat file and/or system debug log
// via OutputDebugString.
// 
// 日志记录输出介质
enum LoggingDestination {
    LOG_NONE                = 0,
    LOG_TO_FILE             = 1 << 0,   // 写入日志文件
    LOG_TO_SYSTEM_DEBUG_LOG = 1 << 1,   // 系统标准输出

    LOG_TO_ALL = LOG_TO_FILE | LOG_TO_SYSTEM_DEBUG_LOG,

    // On Windows, use a file next to the exe; on POSIX platforms, where
    // it may not even be possible to locate the executable on disk, use
    // stderr.
    // 
    // 默认下， WIN 平台使用日志文件， POSIX 平台使用标准错误输出 stderr
#if defined(OS_WIN)
    LOG_DEFAULT = LOG_TO_FILE,
#elif defined(OS_POSIX)
    LOG_DEFAULT = LOG_TO_SYSTEM_DEBUG_LOG,
#endif
};

// Indicates that the log file should be locked when being written to.
// Unless there is only one single-threaded process that is logging to
// the log file, the file should be locked during writes to make each
// log output atomic. Other writers will block.
//
// All processes writing to the log file must have their locking set for it to
// work properly. Defaults to LOCK_LOG_FILE.
// 
// 日志文件在写入时应被锁定。除非是单线程单进程在记录日志
// 
// 是否所有线程写入日志文件时必须设置他们 "锁定状态" 才能工作。默认需要"锁定状态"
enum LogLockingState { LOCK_LOG_FILE, DONT_LOCK_LOG_FILE };

// On startup, should we delete or append to an existing log file (if any)?
// Defaults to APPEND_TO_OLD_LOG_FILE.
// 
// 初始化日志时，我们应该以 "删除或者附加" 的方式处理旧日志文件。默认以 "附加" 方式
enum OldFileDeletionState { DELETE_OLD_LOG_FILE, APPEND_TO_OLD_LOG_FILE };

// 日志名称和其他日志配置项
struct BUTIL_EXPORT LoggingSettings {
    // The defaults values are:
    //
    //  logging_dest: LOG_DEFAULT
    //  log_file:     NULL
    //  lock_log:     LOCK_LOG_FILE
    //  delete_old:   APPEND_TO_OLD_LOG_FILE
    LoggingSettings();

    LoggingDestination logging_dest;

    // The three settings below have an effect only when LOG_TO_FILE is
    // set in |logging_dest|.
    // 
    // 以下三个设置仅在 |logging_dest| 设置为 LOG_TO_FILE 时才起作用
    const PathChar* log_file;           // 日志文件名
    LogLockingState lock_log;           // 写入日志时是否需锁文件
    OldFileDeletionState delete_old;    // 初始化日志时，是否删除旧文件
};

// Implementation of the InitLogging() method declared below. 
// 
// 日志初始化。 InitLogging() 的底层实现接口
BUTIL_EXPORT bool BaseInitLoggingImpl(const LoggingSettings& settings);

// Sets the log file name and other global logging state. Calling this function
// is recommended, and is normally done at the beginning of application init.
// If you don't call it, all the flags will be initialized to their default
// values, and there is a race condition that may leak a critical section
// object if two threads try to do the first log at the same time.
// See the definition of the enums above for descriptions and default values.
//
// The default log file is initialized to "<process-name>.log" on linux and
// "debug.log" otherwise.
//
// This function may be called a second time to re-direct logging (e.g after
// loging in to a user partition), however it should never be called more than
// twice.
// 
// 设置日志文件名和其他全局日志记录状态。建议调用此函数，通常在应用程序 init 开始时完成。
// 如果不调用它，所有的标志将被初始化为其默认值，如果两个线程尝试同时执行一个日志，则有一
// 个竞争条件可能会泄漏关键部分对象的处理。
inline bool InitLogging(const LoggingSettings& settings) {
    return BaseInitLoggingImpl(settings);
}

// Sets the log level. Anything at or above this level will be written to the
// log file/displayed to the user (if applicable). Anything below this level
// will be silently ignored. The log level defaults to 0 (everything is logged
// up to level INFO) if this function is not called.
// 
// 设置日志级别。该级别以上的任何内容将被写入日志文件/显示给用户（如果使用）。任何低于此级
// 别的日志都将被忽略。如果未调用此功能，日志级别默认为 0（所有内容高于 INFO 级别）
BUTIL_EXPORT void SetMinLogLevel(int level);

// Gets the current log level.
// 
// 当前日志最小警告级别
BUTIL_EXPORT int GetMinLogLevel();

// Sets whether or not you'd like to see fatal debug messages popped up in
// a dialog box or not.
// Dialogs are not shown by default.
// 
// 设置是否希望在出现致命错误调试消息时弹出对话框。默认不显示对话框
BUTIL_EXPORT void SetShowErrorDialogs(bool enable_dialogs);

// Sets the Log Assert Handler that will be used to notify of check failures.
// The default handler shows a dialog box and then terminate the process,
// however clients can use this function to override with their own handling
// (e.g. a silent one for Unit Tests)
// 
// 设置将用于通知检查失败的 Log Assert 处理程序。默认处理程序显示一个对话框，然后终止
// 进程。但是客户端可以使用此函数来用自己的处理来覆盖此设置。
typedef void (*LogAssertHandler)(const std::string& str);
BUTIL_EXPORT void SetLogAssertHandler(LogAssertHandler handler);

// LogSink 日志接收器。主要用于单元测试。Logging 都会设置它
class LogSink {
public:
    LogSink() {}
    virtual ~LogSink() {}
    // Called when a log is ready to be written out.
    // Returns true to stop further processing.
    // 
    // 当日志准备好输出时调用。 返回 true 以停止进一步处理
    virtual bool OnLogMessage(int severity, const char* file, int line,
                              const butil::StringPiece& log_content) = 0;
private:
    DISALLOW_COPY_AND_ASSIGN(LogSink);
};

// Sets the LogSink that gets passed every log message before
// it's sent to default log destinations.
// This function is thread-safe and waits until current LogSink is not used
// anymore.
// Returns previous sink.
// 
// 设置在发送到默认日志目标之前每个日志消息传递的 LogSink
// 此函数是线程安全的，并等待直到当前的 LogSink 不再被使用
BUTIL_EXPORT LogSink* SetLogSink(LogSink* sink);

// The LogSink mainly for unit-testing. Logs will be appended to it.
class StringSink : public LogSink, public std::string {
public:
    bool OnLogMessage(int severity, const char* file, int line,
                 const butil::StringPiece& log_content);
private:
    butil::Lock _lock;
};

// @tips
// NDEBUG 宏是 Standard C 中定义的宏，专门用来控制 assert() 的行为。默认状态下没
// 有定义 NDEBUG ，此时 assert 将执行运行时检查。如果定义了这个宏，则 assert 不会
// 起作用。即：非调试模式（发布版本）关闭断言功能。在 VC++ 里面， release 会在全局
// 定义 NDEBUG。也可以修改 makefile 的方式来全局定义 NDEBUG 。


// 警告进别越来越高（越来越严格）。 VERBOSE 为最低级别的日志：输出所有日志信息。
// DEBUG/DFATAL(BLOG_DEBUG,BLOG_DFATAL) 级别随 NDEBUG 宏定义而变化。这样定义可实
// 现日志警告级别的一键定制化。尽量使用它们！
typedef int LogSeverity;
const LogSeverity BLOG_VERBOSE = -1;  // This is level 1 verbosity
// Note: the log severities are used to index into the array of names,
// see log_severity_names.
const LogSeverity BLOG_INFO = 0;
const LogSeverity BLOG_NOTICE = 1;
const LogSeverity BLOG_WARNING = 2;
const LogSeverity BLOG_ERROR = 3;
const LogSeverity BLOG_FATAL = 4;
const int LOG_NUM_SEVERITIES = 5;

// COMBLOG_TRACE is just INFO
// 
// TRACE 级别与 INFO 相同
const LogSeverity BLOG_TRACE = BLOG_INFO;

// COMBLOG_DEBUG equals INFO in debug mode and verbose in normal mode.
// 
// 非调试模式（发布版本）中， DEBUG 级别与 INFO 相同。否则，输出所有日志信息。
#ifndef NDEBUG
const LogSeverity BLOG_DEBUG = BLOG_INFO;
#else
const LogSeverity BLOG_DEBUG = BLOG_VERBOSE;
#endif

// BLOG_DFATAL is BLOG_FATAL in debug mode, ERROR in normal mode
// 
// 非调试模式（发布版本）中， DFATAL 级别与 FATAL 相同。否则，输出 ERROR 级别错
// 误（放低一个级别，尽可能在调试模式中找出所有错误）。
#ifndef NDEBUG
const LogSeverity BLOG_DFATAL = BLOG_FATAL;
#else
const LogSeverity BLOG_DFATAL = BLOG_ERROR;
#endif

// A few definitions of macros that don't generate much code. These are used
// by LOG() and LOG_IF, etc. Since these are used all over our code, it's
// better to have compact code for these operations.
// 
// 创建指定日志对象。 ClassName 对象类型。
// 一些不生成代码的宏的定义。这些由 LOG() 和 LOG_IF() 等宏使用。由于这些都在我们的代码
// 中使用，最好是为这些操作提供紧凑的代码。
#define BAIDU_COMPACT_LOG_EX(severity, ClassName, ...)  \
    ::logging::ClassName(__FILE__, __LINE__,            \
    ::logging::BLOG_##severity, ##__VA_ARGS__)

// 默认创建 ::logging::LogMessage 日志对象
#define BAIDU_COMPACK_LOG(severity)             \
    BAIDU_COMPACT_LOG_EX(severity, LogMessage)

#if defined(OS_WIN)
// wingdi.h defines ERROR to be 0. When we call LOG(ERROR), it gets
// substituted with 0, and it expands to BAIDU_COMPACK_LOG(0). To allow us
// to keep using this syntax, we define this macro to do the same thing
// as BAIDU_COMPACK_LOG(ERROR), and also define ERROR the same way that
// the Windows SDK does for consistency.
// 
// wingdi.h 将 ERROR 定义为 0 。当我们调用 LOG(ERROR) 时，它将被替换为 0 ，并将
// 其扩展为 BAIDU_COMPACK_LOG(0) 。为了让我们继续使用这种语法，我们定义这个宏来和 
// BAIDU_COMPACK_LOG(ERROR) 做同样的事情，并且同样的定义 ERROR 与 Windows SDK 
// 的一致。
#undef ERROR
#define ERROR 0
// Needed for LOG_IS_ON(ERROR).
const LogSeverity BLOG_0 = BLOG_ERROR;
#endif

// As special cases, we can assume that LOG_IS_ON(FATAL) always holds. Also,
// LOG_IS_ON(DFATAL) always holds in debug mode. In particular, CHECK()s will
// always fire if they fail.
// 
// 是否开启记录调试日志
// 作为特例，我们可以假定 LOG_IS_ON(FATAL) 总是成立。此外， LOG_IS_ON(DFATAL) 始终
// 保持调试模式。特别是，如果 CHECK()s 失败，它总是会被触发。
#define LOG_IS_ON(severity)                                     \
    (logging::BLOG_##severity >= ::logging::GetMinLogLevel())

// @todo
// verbose log is on 是否开启宏?
#if defined(__GNUC__)
// We emit an anonymous static int* variable at every VLOG_IS_ON(n) site.
// (Normally) the first time every VLOG_IS_ON(n) site is hit,
// we determine what variable will dynamically control logging at this site:
// it's either FLAGS_verbose or an appropriate internal variable
// matching the current source file that represents results of
// parsing of --vmodule flag and/or SetVLOGLevel calls.
// 
// 我们在每个 VLOG_IS_ON(n) 站点发出一个匿名静态 int* 变量。（通常情况下）每次点击 
// VLOG_IS_ON(n) 站点时，我们确定哪个变量会动态控制该站点的日志记录：它是 FLAGS_verbose 
// 或与当前源文件匹配的适当内部变量，表示解析 --vmodule 标志的结果和 / 或 SetVLOGLevel 
// 调用。
# define BAIDU_VLOG_IS_ON(verbose_level, filepath)                      \
    ({ static const int* vlocal = &::logging::VLOG_UNINITIALIZED;       \
        const int saved_verbose_level = (verbose_level);                \
        (saved_verbose_level >= 0)/*VLOG(-1) is forbidden*/ &&          \
            (*vlocal >= saved_verbose_level) &&                         \
            ((vlocal != &::logging::VLOG_UNINITIALIZED) ||              \
             (::logging::add_vlog_site(&vlocal, filepath, __LINE__,     \
                                       saved_verbose_level))); })
#else
// GNU extensions not available, so we do not support --vmodule.
// Dynamic value of FLAGS_verbose always controls the logging level.
# define BAIDU_VLOG_IS_ON(verbose_level, filepath)      \
    (::logging::FLAGS_v >= (verbose_level))
#endif

#define VLOG_IS_ON(verbose_level) BAIDU_VLOG_IS_ON(verbose_level, __FILE__)

// 声明命令行 v 参数。控制 VLOG_IS_ON(verbose_level) 日志级别
DECLARE_int32(v);

extern const int VLOG_UNINITIALIZED;

// Called to initialize a VLOG callsite.
// 
// 初始化 VLOG 站点
bool add_vlog_site(const int** v, const PathChar* filename, int line_no,
                   int required_v);

class VLogSitePrinter {
public:
    struct Site {
        int current_verbose_level;
        int required_verbose_level;
        int line_no;
        std::string full_module;
    };

    virtual void print(const Site& site) = 0;
};

void print_vlog_sites(VLogSitePrinter*);

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold.
// 
// 如果 condition 条件不成立，辅助宏将避免 stream 流的参数。
// 如果 condition 条件不成立，辅助宏 Helper 将不检查 stream 流参数。否则，
// 检查 stream 必须是一个 std::ostream 流。
#define BAIDU_LAZY_STREAM(stream, condition)                            \
    !(condition) ? (void) 0 : ::logging::LogMessageVoidify() & (stream)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token BAIDU_COMPACK_LOG(INFO).  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
// 
// 获取日志流对象。
// LOG(INFO) 标记将变成 BAIDU_COMPACK_LOG(INFO) ，进而创建 ::logging::LogMessage 
// 日志对象。 ostream 成员函数（例如 ostream::operator<<(int) ） 和 ostream 非成员
// 函数（例如 ::operator<<(ostream&, string&) ）之间存在一些有趣的细微差别：事实证明，
// 我们通过调用 LogMessage::stream() 成员函数来实现一个整洁的 hack ，来避免这个问题
#define LOG_STREAM(severity) BAIDU_COMPACK_LOG(severity).stream()

// 创建日志流，并根据日志级别验证流的合法性：必须是 std::ostream 的流
#define LOG(severity)                                                   \
    BAIDU_LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity))
#define LOG_IF(severity, condition)                                     \
    BAIDU_LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

// FIXME(gejun): Should always crash.
// 
// 日志断言（级别为 FATAL ）
#define LOG_ASSERT(condition)                                           \
    LOG_IF(FATAL, !(condition)) << "Assert failed: " #condition ". "

#define SYSLOG(severity) LOG(severity)
#define SYSLOG_IF(severity, condition) LOG_IF(severity, condition)
#define SYSLOG_EVERY_N(severity, N) LOG_EVERY_N(severity, N)
#define SYSLOG_IF_EVERY_N(severity, condition, N) LOG_IF_EVERY_N(severity, condition, N)
#define SYSLOG_FIRST_N(severity, N) LOG_FIRST_N(severity, N)
#define SYSLOG_IF_FIRST_N(severity, condition, N) LOG_IF_FIRST_N(severity, condition, N)
#define SYSLOG_ONCE(severity) LOG_FIRST_N(severity, 1)
#define SYSLOG_IF_ONCE(severity, condition) LOG_IF_FIRST_N(severity, condition, 1)
#define SYSLOG_EVERY_SECOND(severity) LOG_EVERY_SECOND(severity)
#define SYSLOG_IF_EVERY_SECOND(severity, condition) LOG_IF_EVERY_SECOND(severity, condition)

// 系统日志断言（级别为 FATAL ）
#define SYSLOG_ASSERT(condition)                                        \
    SYSLOG_IF(FATAL, !(condition)) << "Assert failed: " #condition ". "

// file/line can be specified at running-time. This is useful for printing
// logs with known file/line inside a LogSink or LogMessageHandler
// 
// 创建运行期可指定 file/line 的日志流对象。
#define LOG_AT_STREAM(severity, file, line)                             \
    ::logging::LogMessage(file, line, ::logging::BLOG_##severity).stream()

// 创建日志流，并根据日志级别验证流的合法性：必须是 std::ostream 的流。
#define LOG_AT(severity, file, line)                                    \
    BAIDU_LAZY_STREAM(LOG_AT_STREAM(severity, file, line), LOG_IS_ON(severity))

// The VLOG macros log with negative verbosities.
// 
// 创建负数警告级别的日志流
#define VLOG_STREAM(verbose_level)                                      \
    ::logging::LogMessage(__FILE__, __LINE__, -(verbose_level)).stream()

// 创建日志流，并根据日志级别验证流的合法性：必须是 std::ostream 的流。
#define VLOG(verbose_level)                                             \
    BAIDU_LAZY_STREAM(VLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))
#define VLOG_IF(verbose_level, condition)                       \
    BAIDU_LAZY_STREAM(VLOG_STREAM(verbose_level),               \
                      VLOG_IS_ON(verbose_level) && (condition))

#define VLOG_EVERY_N(verbose_level, N)                                  \
    BAIDU_LOG_IF_EVERY_N_IMPL(VLOG_IF, verbose_level, true, N)
#define VLOG_IF_EVERY_N(verbose_level, condition, N)                    \
    BAIDU_LOG_IF_EVERY_N_IMPL(VLOG_IF, verbose_level, condition, N)

#define VLOG_FIRST_N(verbose_level, N)                                  \
    BAIDU_LOG_IF_FIRST_N_IMPL(VLOG_IF, verbose_level, true, N)
#define VLOG_IF_FIRST_N(verbose_level, condition, N)                    \
    BAIDU_LOG_IF_FIRST_N_IMPL(VLOG_IF, verbose_level, condition, N)

#define VLOG_ONCE(verbose_level) VLOG_FIRST_N(verbose_level, 1)
#define VLOG_IF_ONCE(verbose_level, condition) VLOG_IF_FIRST_N(verbose_level, condition, 1)

#define VLOG_EVERY_SECOND(verbose_level)                        \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(VLOG_IF, verbose_level, true)
#define VLOG_IF_EVERY_SECOND(verbose_level, condition)                  \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(VLOG_IF, verbose_level, condition)

// ::logging::Win32ErrorLogMessage|::logging::ErrnoLogMessage 类型日志对象
#if defined (OS_WIN)
#define VPLOG_STREAM(verbose_level)                                     \
     ::logging::Win32ErrorLogMessage(__FILE__, __LINE__, -verbose_level, \
                                     ::logging::GetLastSystemErrorCode()).stream()
#elif defined(OS_POSIX)
#define VPLOG_STREAM(verbose_level)                                     \
    ::logging::ErrnoLogMessage(__FILE__, __LINE__, -verbose_level,      \
                               ::logging::GetLastSystemErrorCode()).stream()
#endif

#define VPLOG(verbose_level)                                            \
    BAIDU_LAZY_STREAM(VPLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))

#define VPLOG_IF(verbose_level, condition)                      \
    BAIDU_LAZY_STREAM(VPLOG_STREAM(verbose_level),              \
                      VLOG_IS_ON(verbose_level) && (condition))

#if defined(OS_WIN)
#define PLOG_STREAM(severity)                                           \
    BAIDU_COMPACT_LOG_EX(severity, Win32ErrorLogMessage,                \
                         ::logging::GetLastSystemErrorCode()).stream()
#elif defined(OS_POSIX)
#define PLOG_STREAM(severity)                                           \
    BAIDU_COMPACT_LOG_EX(severity, ErrnoLogMessage,                     \
                         ::logging::GetLastSystemErrorCode()).stream()
#endif

#define PLOG(severity)                                                  \
    BAIDU_LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity))
#define PLOG_IF(severity, condition)                                    \
    BAIDU_LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

// The actual stream used isn't important.
#define BAIDU_EAT_STREAM_PARAMS                                           \
    true ? (void) 0 : ::logging::LogMessageVoidify() & LOG_STREAM(FATAL)

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by NDEBUG, so the check will be executed regardless of
// compilation mode.
//
// We make sure CHECK et al. always evaluates their arguments, as
// doing CHECK(FunctionWithSideEffect()) is a common idiom.
// 
// 如果条件不成立，CHECK 会崩溃并带有致命错误。 它不受 NDEBUG 控制，因此无论编译模
// 式如何，都将执行检查。（ assert() 受 NDEBUG 影响）
// 
// CHECK 总是评估它们的参数， CHECK(FunctionWithSideEffect()) 是一个常见的习惯
// 用法。

#if defined(OFFICIAL_BUILD) && defined(NDEBUG)

// Make all CHECK functions discard their log strings to reduce code
// bloat for official release builds.

// TODO(akalin): This would be more valuable if there were some way to
// remove BreakDebugger() from the backtrace, perhaps by turning it
// into a macro (like __debugbreak() on Windows).
#define CHECK(condition)                                                \
    !(condition) ? ::butil::debug::BreakDebugger() : BAIDU_EAT_STREAM_PARAMS

#define PCHECK(condition) CHECK(condition)

#define BAIDU_CHECK_OP(name, op, val1, val2) CHECK((val1) op (val2))

#else

#define CHECK(condition)                                        \
    BAIDU_LAZY_STREAM(LOG_STREAM(FATAL).SetCheck(), !(condition))     \
    << "Check failed: " #condition ". "

#define PCHECK(condition)                                       \
    BAIDU_LAZY_STREAM(PLOG_STREAM(FATAL).SetCheck(), !(condition))    \
    << "Check failed: " #condition ". "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.
//
// TODO(akalin): Rewrite this so that constructs like if (...)
// CHECK_EQ(...) else { ... } work properly.
#define BAIDU_CHECK_OP(name, op, val1, val2)                                  \
    if (std::string* _result =                                          \
        ::logging::Check##name##Impl((val1), (val2),                    \
                                     #val1 " " #op " " #val2))          \
        ::logging::LogMessage(__FILE__, __LINE__, _result).stream().SetCheck()

#endif

// Build the error message string.  This is separate from the "Impl"
// function template because it is not performance critical and so can
// be out of line, while the "Impl" code should be inline.  Caller
// takes ownership of the returned string.
// 
// 生成错误消息字符串。这与 "Impl" 函数模板是分开的，因为它不是性能关键的，因此可能脱节，
// 而 "Impl" 代码应该是内联的。调用者获取返回的字符串的所有权（需 delete 字符串）
template<class t1, class t2>
std::string* MakeCheckOpString(const t1& v1, const t2& v2, const char* names) {
    std::ostringstream ss;
    ss << names << " (" << v1 << " vs " << v2 << "). ";
    std::string* msg = new std::string(ss.str());
    return msg;
}

// MSVC doesn't like complex extern templates and DLLs.
// 
// 实例化声明外部链接模版
#if !defined(COMPILER_MSVC)
// Commonly used instantiations of MakeCheckOpString<>. Explicitly instantiated
// in logging.cc.
extern template BUTIL_EXPORT std::string* MakeCheckOpString<int, int>(
    const int&, const int&, const char* names);
extern template BUTIL_EXPORT
std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&, const unsigned long&, const char* names);
extern template BUTIL_EXPORT
std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&, const unsigned int&, const char* names);
extern template BUTIL_EXPORT
std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&, const unsigned long&, const char* names);
extern template BUTIL_EXPORT
std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&, const std::string&, const char* name);
#endif

// Helper functions for BAIDU_CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below.
#define BAIDU_DEFINE_CHECK_OP_IMPL(name, op)                            \
    template <class t1, class t2>                                       \
    inline std::string* Check##name##Impl(const t1& v1, const t2& v2,   \
                                          const char* names) {          \
        if (v1 op v2) return NULL;                                      \
        else return MakeCheckOpString(v1, v2, names);                   \
    }                                                                   \
    inline std::string* Check##name##Impl(int v1, int v2, const char* names) { \
        if (v1 op v2) return NULL;                                      \
        else return MakeCheckOpString(v1, v2, names);                   \
    }
BAIDU_DEFINE_CHECK_OP_IMPL(EQ, ==)
BAIDU_DEFINE_CHECK_OP_IMPL(NE, !=)
BAIDU_DEFINE_CHECK_OP_IMPL(LE, <=)
BAIDU_DEFINE_CHECK_OP_IMPL(LT, < )
BAIDU_DEFINE_CHECK_OP_IMPL(GE, >=)
BAIDU_DEFINE_CHECK_OP_IMPL(GT, > )
#undef BAIDU_DEFINE_CHECK_OP_IMPL

// 关系运算 "断言" 宏
#define CHECK_EQ(val1, val2) BAIDU_CHECK_OP(EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) BAIDU_CHECK_OP(NE, !=, val1, val2)
#define CHECK_LE(val1, val2) BAIDU_CHECK_OP(LE, <=, val1, val2)
#define CHECK_LT(val1, val2) BAIDU_CHECK_OP(LT, < , val1, val2)
#define CHECK_GE(val1, val2) BAIDU_CHECK_OP(GE, >=, val1, val2)
#define CHECK_GT(val1, val2) BAIDU_CHECK_OP(GT, > , val1, val2)

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define DCHECK_IS_ON() 0
#else
#define DCHECK_IS_ON() 1
#endif

#define ENABLE_DLOG DCHECK_IS_ON()

// Definitions for DLOG et al.

// Need to be this way because `condition' may contain variables that is only
// defined in debug mode.
// 
// 受 NDEBUG 影响的 带 `condition' 的日志宏
#if ENABLE_DLOG
#define DLOG_IS_ON(severity) LOG_IS_ON(severity)
#define DLOG_IF(severity, condition)                    \
    LOG_IF(severity, ENABLE_DLOG && (condition))
#define DLOG_ASSERT(condition) LOG_ASSERT(!ENABLE_DLOG || condition)
#define DPLOG_IF(severity, condition)                   \
    PLOG_IF(severity, ENABLE_DLOG && (condition))
#define DVLOG_IF(verbose_level, condition)               \
    VLOG_IF(verbose_level, ENABLE_DLOG && (condition))
#define DVPLOG_IF(verbose_level, condition)      \
    VPLOG_IF(verbose_level, ENABLE_DLOG && (condition))
#else  // ENABLE_DLOG
#define DLOG_IS_ON(severity) false
#define DLOG_IF(severity, condition) BAIDU_EAT_STREAM_PARAMS
#define DLOG_ASSERT(condition) BAIDU_EAT_STREAM_PARAMS
#define DPLOG_IF(severity, condition) BAIDU_EAT_STREAM_PARAMS
#define DVLOG_IF(verbose_level, condition) BAIDU_EAT_STREAM_PARAMS
#define DVPLOG_IF(verbose_level, condition) BAIDU_EAT_STREAM_PARAMS
#endif  // ENABLE_DLOG

#define DLOG(severity)                                          \
    BAIDU_LAZY_STREAM(LOG_STREAM(severity), DLOG_IS_ON(severity))
#define DLOG_EVERY_N(severity, N)                               \
    BAIDU_LOG_IF_EVERY_N_IMPL(DLOG_IF, severity, true, N)
#define DLOG_IF_EVERY_N(severity, condition, N)                 \
    BAIDU_LOG_IF_EVERY_N_IMPL(DLOG_IF, severity, condition, N)
#define DLOG_FIRST_N(severity, N)                               \
    BAIDU_LOG_IF_FIRST_N_IMPL(DLOG_IF, severity, true, N)
#define DLOG_IF_FIRST_N(severity, condition, N)                 \
    BAIDU_LOG_IF_FIRST_N_IMPL(DLOG_IF, severity, condition, N)
#define DLOG_ONCE(severity) DLOG_FIRST_N(severity, 1)
#define DLOG_IF_ONCE(severity, condition) DLOG_IF_FIRST_N(severity, condition, 1)
#define DLOG_EVERY_SECOND(severity)                             \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(DLOG_IF, severity, true)
#define DLOG_IF_EVERY_SECOND(severity, condition)                       \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(DLOG_IF, severity, condition)

#define DPLOG(severity)                                         \
    BAIDU_LAZY_STREAM(PLOG_STREAM(severity), DLOG_IS_ON(severity))
#define DPLOG_EVERY_N(severity, N)                               \
    BAIDU_LOG_IF_EVERY_N_IMPL(DPLOG_IF, severity, true, N)
#define DPLOG_IF_EVERY_N(severity, condition, N)                 \
    BAIDU_LOG_IF_EVERY_N_IMPL(DPLOG_IF, severity, condition, N)
#define DPLOG_FIRST_N(severity, N)                               \
    BAIDU_LOG_IF_FIRST_N_IMPL(DPLOG_IF, severity, true, N)
#define DPLOG_IF_FIRST_N(severity, condition, N)                 \
    BAIDU_LOG_IF_FIRST_N_IMPL(DPLOG_IF, severity, condition, N)
#define DPLOG_ONCE(severity) DPLOG_FIRST_N(severity, 1)
#define DPLOG_IF_ONCE(severity, condition) DPLOG_IF_FIRST_N(severity, condition, 1)
#define DPLOG_EVERY_SECOND(severity)                             \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(DPLOG_IF, severity, true)
#define DPLOG_IF_EVERY_SECOND(severity, condition)                       \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(DPLOG_IF, severity, condition)

#define DVLOG(verbose_level) DVLOG_IF(verbose_level, VLOG_IS_ON(verbose_level))
#define DVLOG_EVERY_N(verbose_level, N)                               \
    BAIDU_LOG_IF_EVERY_N_IMPL(DVLOG_IF, verbose_level, true, N)
#define DVLOG_IF_EVERY_N(verbose_level, condition, N)                 \
    BAIDU_LOG_IF_EVERY_N_IMPL(DVLOG_IF, verbose_level, condition, N)
#define DVLOG_FIRST_N(verbose_level, N)                               \
    BAIDU_LOG_IF_FIRST_N_IMPL(DVLOG_IF, verbose_level, true, N)
#define DVLOG_IF_FIRST_N(verbose_level, condition, N)                 \
    BAIDU_LOG_IF_FIRST_N_IMPL(DVLOG_IF, verbose_level, condition, N)
#define DVLOG_ONCE(verbose_level) DVLOG_FIRST_N(verbose_level, 1)
#define DVLOG_IF_ONCE(verbose_level, condition) DVLOG_IF_FIRST_N(verbose_level, condition, 1)
#define DVLOG_EVERY_SECOND(verbose_level)                             \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(DVLOG_IF, verbose_level, true)
#define DVLOG_IF_EVERY_SECOND(verbose_level, condition)                       \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(DVLOG_IF, verbose_level, condition)

#define DVPLOG(verbose_level) DVPLOG_IF(verbose_level, VLOG_IS_ON(verbose_level))
#define DVPLOG_EVERY_N(verbose_level, N)                               \
    BAIDU_LOG_IF_EVERY_N_IMPL(DVPLOG_IF, verbose_level, true, N)
#define DVPLOG_IF_EVERY_N(verbose_level, condition, N)                 \
    BAIDU_LOG_IF_EVERY_N_IMPL(DVPLOG_IF, verbose_level, condition, N)
#define DVPLOG_FIRST_N(verbose_level, N)                               \
    BAIDU_LOG_IF_FIRST_N_IMPL(DVPLOG_IF, verbose_level, true, N)
#define DVPLOG_IF_FIRST_N(verbose_level, condition, N)                 \
    BAIDU_LOG_IF_FIRST_N_IMPL(DVPLOG_IF, verbose_level, condition, N)
#define DVPLOG_ONCE(verbose_level) DVPLOG_FIRST_N(verbose_level, 1)
#define DVPLOG_IF_ONCE(verbose_level, condition) DVPLOG_IF_FIRST_N(verbose_level, condition, 1)
#define DVPLOG_EVERY_SECOND(verbose_level)                             \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(DVPLOG_IF, verbose_level, true)
#define DVPLOG_IF_EVERY_SECOND(verbose_level, condition)                       \
    BAIDU_LOG_IF_EVERY_SECOND_IMPL(DVPLOG_IF, verbose_level, condition)

// You can assign virtual path to VLOG instead of physical filename.
// [public/foo/bar.cpp]
// VLOG2("a/b/c", 2) << "being filtered by a/b/c rather than public/foo/bar";
// 
// 支持将虚拟路径分配给 VLOG ，而不是物理文件名。
#define VLOG2(virtual_path, verbose_level)                              \
    BAIDU_LAZY_STREAM(VLOG_STREAM(verbose_level),                       \
                      BAIDU_VLOG_IS_ON(verbose_level, virtual_path))

#define VLOG2_IF(virtual_path, verbose_level, condition)                \
    BAIDU_LAZY_STREAM(VLOG_STREAM(verbose_level),                       \
                      BAIDU_VLOG_IS_ON(verbose_level, virtual_path) && (condition))

#define DVLOG2(virtual_path, verbose_level)             \
    VLOG2_IF(virtual_path, verbose_level, ENABLE_DLOG)

#define DVLOG2_IF(virtual_path, verbose_level, condition)               \
    VLOG2_IF(virtual_path, verbose_level, ENABLE_DLOG && (condition))

#define VPLOG2(virtual_path, verbose_level)                             \
    BAIDU_LAZY_STREAM(VPLOG_STREAM(verbose_level),                      \
                      BAIDU_VLOG_IS_ON(verbose_level, virtual_path))

#define VPLOG2_IF(virtual_path, verbose_level, condition)               \
    BAIDU_LAZY_STREAM(VPLOG_STREAM(verbose_level),                      \
                      BAIDU_VLOG_IS_ON(verbose_level, virtual_path) && (condition))

#define DVPLOG2(virtual_path, verbose_level)                            \
    VPLOG2_IF(virtual_path, verbose_level, ENABLE_DLOG)

#define DVPLOG2_IF(virtual_path, verbose_level, condition)              \
    VPLOG2_IF(virtual_path, verbose_level, ENABLE_DLOG && (condition))

// Definitions for DCHECK et al.

#if DCHECK_IS_ON()

const LogSeverity BLOG_DCHECK = BLOG_FATAL;

#else  // DCHECK_IS_ON

const LogSeverity BLOG_DCHECK = BLOG_INFO;

#endif  // DCHECK_IS_ON

// DCHECK et al. make sure to reference |condition| regardless of
// whether DCHECKs are enabled; this is so that we don't get unused
// variable warnings if the only use of a variable is in a DCHECK.
// This behavior is different from DLOG_IF et al.
// 
// 务必引用 |condition| 不管 DCHECK 是否启用; 这是因为如果变量的唯一用途就
// 是在 DCHECK 宏中，我们不会得到未使用的变量警告。这种行为不同于 DLOG_IF

#define DCHECK(condition)                                               \
    BAIDU_LAZY_STREAM(LOG_STREAM(DCHECK), DCHECK_IS_ON() && !(condition)) \
    << "Check failed: " #condition ". "

#define DPCHECK(condition)                                              \
    BAIDU_LAZY_STREAM(PLOG_STREAM(DCHECK), DCHECK_IS_ON() && !(condition)) \
    << "Check failed: " #condition ". "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use DCHECK_EQ et al below.
#define BAIDU_DCHECK_OP(name, op, val1, val2)                           \
    if (DCHECK_IS_ON())                                                   \
        if (std::string* _result =                                      \
            ::logging::Check##name##Impl((val1), (val2),                \
                                         #val1 " " #op " " #val2))      \
            ::logging::LogMessage(                                      \
                __FILE__, __LINE__, ::logging::BLOG_DCHECK,             \
                _result).stream()

// Equality/Inequality checks - compare two values, and log a
// BLOG_DCHECK message including the two values when the result is not
// as expected.  The values must have operator<<(ostream, ...)
// defined.
//
// You may append to the error message like so:
//   DCHECK_NE(1, 2) << ": The world must be ending!";
//
// We are very careful to ensure that each argument is evaluated exactly
// once, and that anything which is legal to pass as a function argument is
// legal here.  In particular, the arguments may be temporary expressions
// which will end up being destroyed at the end of the apparent statement,
// for example:
//   DCHECK_EQ(string("abc")[1], 'b');
//
// WARNING: These may not compile correctly if one of the arguments is a pointer
// and the other is NULL. To work around this, simply static_cast NULL to the
// type of the desired pointer.
// 
// Equality/Inequality 比较检查，并在结果不符合预期时记录包含两个值的 BLOG_DCHECK 消息。
// 这些值必须定义 operator<<(ostream, ...)
// Use like:
// DCHECK_NE(1, 2) << ": The world must be ending!";
// 
// 我们非常小心地确保每个参数都被精确地评估一次，并且任何合法的作为函数参数传递的东西在这里都
// 是合法的。特别是，这些论据可能是临时表达式，最终会在声明结束时被销毁。
// Like so:
// DCHECK_EQ(string("abc")[1], 'b');
// 
// 警告：如果其中一个参数是一个指针而另一个是 NULL ，则它们可能无法正确编译。要解决此问题，只
// 需将 static_cast NULL 指定为所需指针的类型

#define DCHECK_EQ(val1, val2) BAIDU_DCHECK_OP(EQ, ==, val1, val2)
#define DCHECK_NE(val1, val2) BAIDU_DCHECK_OP(NE, !=, val1, val2)
#define DCHECK_LE(val1, val2) BAIDU_DCHECK_OP(LE, <=, val1, val2)
#define DCHECK_LT(val1, val2) BAIDU_DCHECK_OP(LT, < , val1, val2)
#define DCHECK_GE(val1, val2) BAIDU_DCHECK_OP(GE, >=, val1, val2)
#define DCHECK_GT(val1, val2) BAIDU_DCHECK_OP(GT, > , val1, val2)

#if defined(OS_WIN)
typedef unsigned long SystemErrorCode;
#elif defined(OS_POSIX)
typedef int SystemErrorCode;
#endif

// Alias for ::GetLastError() on Windows and errno on POSIX. Avoids having to
// pull in windows.h just for GetLastError() and DWORD.
// 
// Windows 上的 ::GetLastError() 和 POSIX 上的 errno 的别名。避免为了使用 GetLastError() 
// 和 DWORD 而拉入 windows.h
BUTIL_EXPORT SystemErrorCode GetLastSystemErrorCode();
BUTIL_EXPORT void SetLastSystemErrorCode(SystemErrorCode err);
BUTIL_EXPORT std::string SystemErrorCodeToString(SystemErrorCode error_code);

// Underlying buffer to store logs. Comparing to using std::ostringstream
// directly, this utility exposes more low-level methods so that we avoid
// creation of std::string which allocates memory internally.
// 
// 底层存储日志的缓冲区。和直接使用 std::ostringstream 相比，这个工具暴露了更多的低
// 级方法，我们可以避免创建 std::string 来在内部分配内存。
class CharArrayStreamBuf : public std::streambuf {
public:
    explicit CharArrayStreamBuf() : _data(NULL), _size(0) {}
    ~CharArrayStreamBuf();

    // 当 _data 缓冲区写满时调用?
    virtual int overflow(int ch);
    virtual int sync();
    void reset();

private:
    char* _data;
    size_t _size;
};

// A std::ostream to << objects.
// Have to use private inheritance to arrange initialization order.
// 
// std::ostream 对象。必须使用私有继承来安排初始化顺序。
class LogStream : virtual private CharArrayStreamBuf, public std::ostream {
friend void DestroyLogStream(LogStream*);
public:
    LogStream()
        : std::ostream(this), _file("-"), _line(0), _severity(0)
        , _noflush(false), _is_check(false) {
    }

    ~LogStream() {
        _noflush = false;
        Flush();
    }

    // operator<< 重载。右侧参数接受 LogStream& (*m)(LogStream&) 类型：回调函数
    inline LogStream& operator<<(LogStream& (*m)(LogStream&)) {
        return m(*this);
    }

    // operator<< 重载。右侧参数接受 LogStream& (*m)(std::ostream&) 类型：回调函数
    inline LogStream& operator<<(std::ostream& (*m)(std::ostream&)) {
        m(*(std::ostream*)this);
        return *this;
    }

    // operator<< 重载。右侧参数接受已重载过 std::ostream::operator<< 类型的数据
    template <typename T> inline LogStream& operator<<(T const& t) {
        *(std::ostream*)this << t;
        return *this;
    }

    // Reset the log prefix: "I0711 15:14:01.830110 12735 server.cpp:93] "
    LogStream& SetPosition(const PathChar* file, int line, LogSeverity);

    // Make FlushIfNeed() no-op once.
    // 
    // 使 FlushIfNeed() 进行一次空操作
    LogStream& DontFlushOnce() {
        _noflush = true;
        return *this;
    }

    LogStream& SetCheck() {
        _is_check = true;
        return *this;
    }

    // 是否为空
    bool empty() const { return pbase() == pptr(); }

    // 返回字符串切片
    butil::StringPiece content() const
    { return butil::StringPiece(pbase(), pptr() - pbase()); }

    // 返回新的 std::string 字符串副本
    std::string content_str() const
    { return std::string(pbase(), pptr() - pbase()); }

    // 返回 文件名、日志行、警告级别
    const PathChar* file() const { return _file; }
    int line() const { return _line; }
    LogSeverity severity() const { return _severity; }

private:
    void FlushWithoutReset();

    // Flush log into sink(if registered) or stderr.
    // NOTE: make this method private to limit the callsites so that the
    // stack-frame removal in FlushWithoutReset() is always safe.
    // 
    // 将日志冲刷新到 sink（如果已注册）中或 stderr 。
    // 注：使此方法为 private 来限制调用，以便 FlushWithoutReset() 中的栈帧移
    // 除始终安全。
    inline void Flush() {
        const bool res = _noflush;
        _noflush = false;
        if (!res) {
            // Save and restore thread-local error code after Flush().
            // 
            // 在 Flush() 后保存并恢复线程本地错误代码
            const SystemErrorCode err = GetLastSystemErrorCode();
            FlushWithoutReset();
            reset();
            clear();
            SetLastSystemErrorCode(err);
            _is_check = false;
        }
    }

    const PathChar* _file;  // 日志文件
    int _line;              // 日志行
    LogSeverity _severity;  // 警告级别
    bool _noflush;          // 是否未刷新缓冲
    bool _is_check;         // 是否检查
};

// This class more or less represents a particular log message.  You
// create an instance of LogMessage and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the
// full message gets streamed to the appropriate destination if `noflush'
// is not present.
//
// You shouldn't actually use LogMessage's constructor to log things,
// though.  You should use the LOG() macro (and variants thereof)
// above.
// 
// 这个类或多或少代表一个特定的日志消息。用来创建一个 LogMessage 的实例，然后
// 将日志写入。当你完成流式传输时，~LogMessage() 被调用，并且如果 'noflush' 
// 不存在时，则完整的消息被流式传输到适当的介质（文件、标准错误）。
// 
// 不过你实际上不应该使用 LogMessage 的构造函数来记录日志。应该使用上面的 LOG() 
// 宏及其变体。
class BUTIL_EXPORT LogMessage {
public:
    // Used for LOG(severity).
    LogMessage(const char* file, int line, LogSeverity severity);

    // Used for CHECK_EQ(), etc. Takes ownership of the given string.
    // Implied severity = BLOG_FATAL.
    LogMessage(const char* file, int line, std::string* result);

    // Used for DCHECK_EQ(), etc. Takes ownership of the given string.
    LogMessage(const char* file, int line, LogSeverity severity,
               std::string* result);

    ~LogMessage();

    LogStream& stream() { return *_stream; }

private:
    DISALLOW_COPY_AND_ASSIGN(LogMessage);

    // The real data is inside LogStream which may be cached thread-locally.
    // 
    // 真正的日志数据在 LogStream 内部，可以是本地线程变量
    LogStream* _stream;
};

// A non-macro interface to the log facility; (useful
// when the logging level is not a compile-time constant).
// 
// 日志设施的普通函数（非宏）接口;（当日志级别不是编译时常量时很有用）
inline void LogAtLevel(int const log_level, const butil::StringPiece &msg) {
    LogMessage(__FILE__, __LINE__, log_level).stream() << msg;
}

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
// 
// 用于显式忽略条件日志宏中的值。这样可避免编译器警告，如“不计算值”和“语句无效”
class LogMessageVoidify {
public:
    LogMessageVoidify() { }
    // This has to be an operator with a precedence lower than << but
    // higher than ?:
    // 
    // 这是一个优先级低于 << 但高于 ?: 的运算符
    void operator&(std::ostream&) { }
};

#if defined(OS_WIN)
// Appends a formatted system message of the GetLastError() type.
// 
// 追加 GetLastError() 格式化后系统消息
class BUTIL_EXPORT Win32ErrorLogMessage {
public:
    Win32ErrorLogMessage(const char* file,
                         int line,
                         LogSeverity severity,
                         SystemErrorCode err);

    // Appends the error message before destructing the encapsulated class.
    // 
    // 在销毁封装的类之前附加错误消息
    ~Win32ErrorLogMessage();

    LogStream& stream() { return log_message_.stream(); }

private:
    SystemErrorCode err_;
    LogMessage log_message_;

    DISALLOW_COPY_AND_ASSIGN(Win32ErrorLogMessage);
};
#elif defined(OS_POSIX)
// Appends a formatted system message of the errno type
// 
// 追加 errno 格式化后系统消息
class BUTIL_EXPORT ErrnoLogMessage {
public:
    ErrnoLogMessage(const char* file,
                    int line,
                    LogSeverity severity,
                    SystemErrorCode err);

    // Appends the error message before destructing the encapsulated class.
    // 
    // 在销毁封装的类之前附加错误消息
    ~ErrnoLogMessage();

    LogStream& stream() { return log_message_.stream(); }

private:
    SystemErrorCode err_;
    LogMessage log_message_;

    DISALLOW_COPY_AND_ASSIGN(ErrnoLogMessage);
};
#endif  // OS_WIN

// Closes the log file explicitly if open.
// NOTE: Since the log file is opened as necessary by the action of logging
//       statements, there's no guarantee that it will stay closed
//       after this call.
// 
// 明确关闭日志文件。注意：由于记录文件的操作会根据需要打开日志文件，因此无法保证在此调
// 用后日志文件将保持关闭状态。
BUTIL_EXPORT void CloseLogFile();

// Async signal safe logging mechanism.
// 
// 异步信号安全日志记录机制
BUTIL_EXPORT void RawLog(int level, const char* message);

#define RAW_LOG(level, message)                         \
    ::logging::RawLog(::logging::BLOG_##level, message)

#define RAW_CHECK(condition, message)                                   \
    do {                                                                \
        if (!(condition))                                               \
            ::logging::RawLog(::logging::BLOG_FATAL, "Check failed: " #condition "\n"); \
    } while (0)

#if defined(OS_WIN)
// Returns the default log file path.
// 
// 返回默认的日志路径
BUTIL_EXPORT std::wstring GetLogFileFullPath();
#endif

inline LogStream& noflush(LogStream& ls) {
    ls.DontFlushOnce();
    return ls;
}

}  // namespace logging

using ::logging::noflush;
using ::logging::VLogSitePrinter;
using ::logging::print_vlog_sites;

// These functions are provided as a convenience for logging, which is where we
// use streams (it is against Google style to use streams in other places). It
// is designed to allow you to emit non-ASCII Unicode strings to the log file,
// which is normally ASCII. It is relatively slow, so try not to use it for
// common cases. Non-ASCII characters will be converted to UTF-8 by these
// operators.
// 
// 提供这些函数是为了方便日志记录，这是我们使用流的地方（在其他地方使用流是违反 Google 风格
// 的）。它旨在允许您将 non-ASCII Unicode 字符串发送到普通的 ASCII 的日志文件。它比较慢，
// 所以尽量不要使用它。这些操作将 Non-ASCII 字符转换为 UTF-8
BUTIL_EXPORT std::ostream& operator<<(std::ostream& out, const wchar_t* wstr);
inline std::ostream& operator<<(std::ostream& out, const std::wstring& wstr) {
    return out << wstr.c_str();
}

// The NOTIMPLEMENTED() macro annotates codepaths which have
// not been implemented yet.
//
// The implementation of this macro is controlled by NOTIMPLEMENTED_POLICY:
//   0 -- Do nothing (stripped by compiler)
//   1 -- Warn at compile time
//   2 -- Fail at compile time
//   3 -- Fail at runtime (DCHECK)
//   4 -- [default] LOG(ERROR) at runtime
//   5 -- LOG(ERROR) at runtime, only once per call-site
//   
// NOTIMPLEMENTED() 宏注释尚未实现的代码路径。该宏的实现由 NOTIMPLEMENTED_POLICY 控制：
// 0 - 什么都不做（由编译器删除）
// 1 - 在编译时警告
// 2 - 编译时失败
// 3 - 运行时失败（DCHECK）
// 4 - [默认]在运行时记录（错误）
// 5 - 运行时记录（错误），每个呼叫站点只有一次

#endif // BRPC_WITH_GLOG

#ifndef NOTIMPLEMENTED_POLICY
#if defined(OS_ANDROID) && defined(OFFICIAL_BUILD)
#define NOTIMPLEMENTED_POLICY 0
#else
// Select default policy: LOG(ERROR)
#define NOTIMPLEMENTED_POLICY 4
#endif
#endif

#if defined(COMPILER_GCC)
// On Linux, with GCC, we can use __PRETTY_FUNCTION__ to get the demangled name
// of the current function in the NOTIMPLEMENTED message.
// 
// 在 Linux上的 GCC，我们可以使用 __PRETTY_FUNCTION__ 在 NOTIMPLEMENTED 消息中获取当
// 前函数的 demangled 名称
#define NOTIMPLEMENTED_MSG "Not implemented reached in " << __PRETTY_FUNCTION__
#else
#define NOTIMPLEMENTED_MSG "NOT IMPLEMENTED"
#endif

#if NOTIMPLEMENTED_POLICY == 0
#define NOTIMPLEMENTED() BAIDU_EAT_STREAM_PARAMS
#elif NOTIMPLEMENTED_POLICY == 1
// TODO, figure out how to generate a warning
#define NOTIMPLEMENTED() COMPILE_ASSERT(false, NOT_IMPLEMENTED)
#elif NOTIMPLEMENTED_POLICY == 2
#define NOTIMPLEMENTED() COMPILE_ASSERT(false, NOT_IMPLEMENTED)
#elif NOTIMPLEMENTED_POLICY == 3
#define NOTIMPLEMENTED() NOTREACHED()
#elif NOTIMPLEMENTED_POLICY == 4
#define NOTIMPLEMENTED() LOG(ERROR) << NOTIMPLEMENTED_MSG
#elif NOTIMPLEMENTED_POLICY == 5
#define NOTIMPLEMENTED() do {                                   \
        static bool logged_once = false;                        \
        LOG_IF(ERROR, !logged_once) << NOTIMPLEMENTED_MSG;      \
        logged_once = true;                                     \
    } while(0);                                                 \
    BAIDU_EAT_STREAM_PARAMS
#endif

#if defined(NDEBUG) && defined(OS_CHROMEOS)
#define NOTREACHED() LOG(ERROR) << "NOTREACHED() hit in "       \
    << __FUNCTION__ << ". "
#else
#define NOTREACHED() DCHECK(false)
#endif

// Helper macro included by all *_EVERY_N macros.
// 
// 第一次调用总是打印日志，而后每 N 次调用后打印日志。 *_EVERY_N 宏的底层实现。
#define BAIDU_LOG_IF_EVERY_N_IMPL(logifmacro, severity, condition, N)   \
    static ::butil::subtle::Atomic32 BAIDU_CONCAT(logeveryn_, __LINE__) = -1; \
    const static int BAIDU_CONCAT(logeveryn_sc_, __LINE__) = (N);       \
    const int BAIDU_CONCAT(logeveryn_c_, __LINE__) =                    \
        ::butil::subtle::NoBarrier_AtomicIncrement(&BAIDU_CONCAT(logeveryn_, __LINE__), 1); \
    logifmacro(severity, (condition) && BAIDU_CONCAT(logeveryn_c_, __LINE__) / \
               BAIDU_CONCAT(logeveryn_sc_, __LINE__) * BAIDU_CONCAT(logeveryn_sc_, __LINE__) \
               == BAIDU_CONCAT(logeveryn_c_, __LINE__))

// Helper macro included by all *_FIRST_N macros.
// 
// 为前 N 次调用打印日志。 *_FIRST_N 宏的底层实现。
#define BAIDU_LOG_IF_FIRST_N_IMPL(logifmacro, severity, condition, N)   \
    static ::butil::subtle::Atomic32 BAIDU_CONCAT(logfstn_, __LINE__) = 0; \
    logifmacro(severity, (condition) && BAIDU_CONCAT(logfstn_, __LINE__) < N && \
               ::butil::subtle::NoBarrier_AtomicIncrement(&BAIDU_CONCAT(logfstn_, __LINE__), 1) <= N)

// Helper macro included by all *_EVERY_SECOND macros.
// 
// 每秒打印一次日志。 *_EVERY_SECOND 宏的底层实现。
#define BAIDU_LOG_IF_EVERY_SECOND_IMPL(logifmacro, severity, condition) \
    static ::butil::subtle::Atomic64 BAIDU_CONCAT(logeverys_, __LINE__) = 0; \
    const int64_t BAIDU_CONCAT(logeverys_ts_, __LINE__) = ::butil::gettimeofday_us(); \
    const int64_t BAIDU_CONCAT(logeverys_seen_, __LINE__) = BAIDU_CONCAT(logeverys_, __LINE__); \
    logifmacro(severity, (condition) && BAIDU_CONCAT(logeverys_ts_, __LINE__) >= \
               (BAIDU_CONCAT(logeverys_seen_, __LINE__) + 1000000L) &&  \
               ::butil::subtle::NoBarrier_CompareAndSwap(                \
                   &BAIDU_CONCAT(logeverys_, __LINE__),                 \
                   BAIDU_CONCAT(logeverys_seen_, __LINE__),             \
                   BAIDU_CONCAT(logeverys_ts_, __LINE__))               \
               == BAIDU_CONCAT(logeverys_seen_, __LINE__))

// ===============================================================

// Print a log for at most once. (not present in glog)
// Almost zero overhead when the log was printed.
// 
// 最多打印一次日志。（不在 glog 中）打印日志时，开销几乎为零
#ifndef LOG_ONCE
# define LOG_ONCE(severity) LOG_FIRST_N(severity, 1)
# define LOG_IF_ONCE(severity, condition) LOG_IF_FIRST_N(severity, condition, 1)
#endif

// Print a log after every N calls. First call always prints.
// Each call to this macro has a cost of relaxed atomic increment.
// The corresponding macro in glog is not thread-safe while this is.
// 
// 第一次调用总是打印日志，而后每 N 次调用后打印日志。每次调用这个宏都会导致原子类型 
// relaxed increment 的代价。 glog 中的相应宏非线程安全。
#ifndef LOG_EVERY_N
# define LOG_EVERY_N(severity, N)                                \
     BAIDU_LOG_IF_EVERY_N_IMPL(LOG_IF, severity, true, N)
# define LOG_IF_EVERY_N(severity, condition, N)                  \
     BAIDU_LOG_IF_EVERY_N_IMPL(LOG_IF, severity, condition, N)
#endif

// Print logs for first N calls.
// Almost zero overhead when the log was printed for N times
// The corresponding macro in glog is not thread-safe while this is.
// 
// 为前 N 次调用打印日志。当日志打印 N 次时，开销几乎为零。 glog 中的相应宏非线程安全
#ifndef LOG_FIRST_N
# define LOG_FIRST_N(severity, N)                                \
     BAIDU_LOG_IF_FIRST_N_IMPL(LOG_IF, severity, true, N)
# define LOG_IF_FIRST_N(severity, condition, N)                  \
     BAIDU_LOG_IF_FIRST_N_IMPL(LOG_IF, severity, condition, N)
#endif

// Print a log every second. (not present in glog). First call always prints.
// Each call to this macro has a cost of calling gettimeofday.
// 
// 每秒打印一次日志。（不在 glog 中）。第一次调用总是打印。每次调用这个宏都会花费一定的
// 时间来调用 gettimeofday()
#ifndef LOG_EVERY_SECOND
# define LOG_EVERY_SECOND(severity)                                \
     BAIDU_LOG_IF_EVERY_SECOND_IMPL(LOG_IF, severity, true)
# define LOG_IF_EVERY_SECOND(severity, condition)                \
     BAIDU_LOG_IF_EVERY_SECOND_IMPL(LOG_IF, severity, condition)
#endif

#ifndef PLOG_EVERY_N
# define PLOG_EVERY_N(severity, N)                               \
     BAIDU_LOG_IF_EVERY_N_IMPL(PLOG_IF, severity, true, N)
# define PLOG_IF_EVERY_N(severity, condition, N)                 \
     BAIDU_LOG_IF_EVERY_N_IMPL(PLOG_IF, severity, condition, N)
#endif

#ifndef PLOG_FIRST_N
# define PLOG_FIRST_N(severity, N)                               \
     BAIDU_LOG_IF_FIRST_N_IMPL(PLOG_IF, severity, true, N)
# define PLOG_IF_FIRST_N(severity, condition, N)                 \
     BAIDU_LOG_IF_FIRST_N_IMPL(PLOG_IF, severity, condition, N)
#endif

#ifndef PLOG_ONCE
# define PLOG_ONCE(severity) PLOG_FIRST_N(severity, 1)
# define PLOG_IF_ONCE(severity, condition) PLOG_IF_FIRST_N(severity, condition, 1)
#endif

#ifndef PLOG_EVERY_SECOND
# define PLOG_EVERY_SECOND(severity)                             \
     BAIDU_LOG_IF_EVERY_SECOND_IMPL(PLOG_IF, severity, true)
# define PLOG_IF_EVERY_SECOND(severity, condition)                       \
     BAIDU_LOG_IF_EVERY_SECOND_IMPL(PLOG_IF, severity, condition)
#endif

// DEBUG_MODE is for uses like
//   if (DEBUG_MODE) foo.CheckThatFoo();
// instead of
//   #ifndef NDEBUG
//     foo.CheckThatFoo();
//   #endif
//
// We tie its state to ENABLE_DLOG.
// 
// DEBUG_MODE 用来取代条件宏检测语句
enum { DEBUG_MODE = DCHECK_IS_ON() };


#endif  // BUTIL_LOGGING_H_

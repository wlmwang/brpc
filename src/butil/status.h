// Copyright (c) 2015 Baidu, Inc.

#ifndef BUTIL_STATUS_H
#define BUTIL_STATUS_H

#include <stdarg.h>                       // va_list
#include <stdlib.h>                       // free
#include <string>                         // std::string
#include <ostream>                        // std::ostream
#include "butil/strings/string_piece.h"

// @tips
// __attribute__((format(printf, m, n)))
// GNU C 特色之一。其中 printf 还可以是 `scanf/strftime/strfmon` 等。其作用是：让编译器按
// 照 printf/scanf/strftime/strfmon 的参数表格式规则对该函数的参数进行检查。即决定参数是哪种
// 风格。编译器会检查函数声明和函数实际调用参数之间的格式化字符串是否匹配。该功能十分有用，尤其是处
// 理一些很难发现的 bug 。
// 
// |m| 为第几个参数为格式化字符串 (format string) （顺序从 1 开始）。
// |n| 为参数集合中的第一个，即参数 "..." 里的第一个参数在函数参数总数排在第几（顺序从 1 开始）。
// 
// Use like:
// extern void myprint(int x，const char *format,...) __attribute__((format(printf, 
// 2, 3)));
// 
// 特别注意的是，如果 myprint 是一个类成员函数，那么 |m| 和 |n| 的值统一要加 +1 。因为类成员方
// 法第一个参数实际上一个 "隐式" 的 "this" 指针。见下面 
// Status::Status(int code, const char* fmt, ...)

namespace butil {

// A Status encapsulates the result of an operation. It may indicate success,
// or it may indicate an error with an associated error message. It's suitable
// for passing status of functions with richer information than just error_code
// in exception-forbidden code. This utility is inspired by leveldb::Status.
//
// Multiple threads can invoke const methods on a Status without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Status must use
// external synchronization.
//
// Since failed status needs to allocate memory, you should be careful when
// failed status is frequent.
// 
// 
// Status 封装了操作的结果。它可以指示成功，也可以带有相关错误消息的失败。适合于传递丰富信息的函
// 数返回，而不仅仅是 exception-forbidden 的 error_code 。灵感来自 leveldb::Status 。
// 
// 多线程可以在 Status 上调用 const 方法而不需要外部同步。但是如果任何线程可能调用 non-const 
// 方法，访问同一个 Status 的所有线程都必须使用外部同步。
// 
// 由于失败的 Status 需要分配内存，所以在频繁构造故障 Status 时应该要小心。
//

class Status {
public:
    struct State {
        // 错误码
        int code;

        // message 大小
        unsigned size;  // length of message string

        // Status::State 自身总大小
        unsigned state_size;

        // 错误消息（柔性数组）
        char message[0];
    };

    // Create a success status.
    // 
    // 创建一个"成功状态"
    Status() : _state(NULL) { }
    // Return a success status.
    // 
    // 返回一个"成功状态"
    static Status OK() { return Status(); }

    ~Status() { reset(); }

    // Create a failed status.
    // error_text is formatted from `fmt' and following arguments.
    // 
    // 创建一个指定的"错误状态"。错误消息 |_state->message| 为使用 `fmt' 
    // 及之后参数格式化的字符串。
    Status(int code, const char* fmt, ...) 
        __attribute__ ((__format__ (__printf__, 3, 4)))
        : _state(NULL) {
        va_list ap;
        va_start(ap, fmt);
        set_errorv(code, fmt, ap);
        va_end(ap);
    }
    Status(int code, const butil::StringPiece& error_msg) : _state(NULL) {
        set_error(code, error_msg);
    }

    // Copy the specified status. Internal fields are deeply copied.
    // 
    // 深度复制构造/赋值构造函数
    Status(const Status& s);
    void operator=(const Status& s);

    // Reset this status to be OK.
    // 
    // 重置到"成功状态"
    void reset();
    
    // Reset this status to be failed.
    // Returns 0 on success, -1 otherwise and internal fields are not changed.
    // 
    // 设置"错误状态"。设置成功返回 0 ，失败返回 -1 ，并且内部状态 |_state| 不改变。
    int set_error(int code, const char* error_format, ...)
        __attribute__ ((__format__ (__printf__, 3, 4)));
    int set_error(int code, const butil::StringPiece& error_msg);
    int set_errorv(int code, const char* error_format, va_list args);

    // Returns true iff the status indicates success.
    // 
    // "成功状态"返回 true
    bool ok() const { return (_state == NULL); }

    // Get the error code
    // 
    // 获取错误码
    int error_code() const {
        return (_state == NULL) ? 0 : _state->code;
    }

    // Return a string representation of the status.
    // Returns "OK" for success.
    // NOTICE:
    //   * You can print a Status to std::ostream directly
    //   * if message contains '\0', error_cstr() will not be shown fully.
    //   
    // 返回一个字符串类型的"状态"。 "OK" 代表成功。注意：
    // * 可以直接打印一个 Status 对象。
    // * error_cstr() 不是二进制安全的。它是 c-style 字符串。
    const char* error_cstr() const {
        return (_state == NULL ? "OK" : _state->message);
    }
    butil::StringPiece error_data() const {
        return (_state == NULL ? butil::StringPiece("OK", 2) 
                : butil::StringPiece(_state->message, _state->size));
    }
    std::string error_str() const;

    void swap(butil::Status& other) { std::swap(_state, other._state); }

private:    
    // OK status has a NULL _state.  Otherwise, _state is a State object
    // converted from malloc().
    // 
    // "状态"的内部结构。 NULL 代表成功状态。
    State* _state;

    static State* copy_state(const State* s);
};

inline Status::Status(const Status& s) {
    _state = (s._state == NULL) ? NULL : copy_state(s._state);
}

inline int Status::set_error(int code, const char* msg, ...) {
    va_list ap;
    va_start(ap, msg);
    const int rc = set_errorv(code, msg, ap);
    va_end(ap);
    return rc;
}

inline void Status::reset() {
    free(_state);
    _state = NULL;
}

inline void Status::operator=(const Status& s) {
    // The following condition catches both aliasing (when this == &s),
    // and the common case where both s and *this are ok.
    if (_state == s._state) {
        return;
    }
    if (s._state == NULL) {
        free(_state);
        _state = NULL;
    } else {
        set_error(s._state->code,
                  butil::StringPiece(s._state->message, s._state->size));
    }
}

inline std::ostream& operator<<(std::ostream& os, const Status& st) {
    // NOTE: don't use st.error_text() which is inaccurate if message has '\0'
    return os << st.error_data();
}

}  // namespace butil

#endif  // BUTIL_STATUS_H

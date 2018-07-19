// Copyright (c) 2015 Baidu, Inc.
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
// Date: Mon Feb  9 15:04:03 CST 2015

#include <stdio.h>
#include "butil/status.h"

// @tips
// \file #include <stdarg.h>
// int vsnprintf(char *str, size_t n, const char *format, va_list ap);
// 将可变参数格式化输出到一个 C 字符串中。即，用于向缓冲区 |str| 字符串中填入数据。执行成
// 功，返回最终生成字符串的长度（不包含终止符）。如果结果字符串的长度超过了 n-1 个字符，则
// 剩余的字符将被丢弃并且不被存储，而是被计算为函数返回的值。同时将原串的长度返回（不包含终
// 止符）；执行失败，返回负值，并设置 errno 。

namespace butil {

inline size_t status_size(size_t message_size) {
    // Add 1 because even if the sum of size is aligned with int, we need to
    // put an ending '\0'
    // 
    // 计算创建指定 |message_size| 错误消息长度的 Status::State 结构需要的内存大小，
    // 并且它是 int 对齐。添加 +1 ，是因为需要放一个结尾 '\0' 字符。
    return ((offsetof(Status::State, message) + message_size)
            / sizeof(int) + 1) * sizeof(int);
}

int Status::set_errorv(int c, const char* fmt, va_list args) {
    if (0 == c) {
        // "成功状态"时， _state == NULL
        free(_state);
        _state = NULL;
        return 0;
    }
    State* new_state = NULL;
    State* state = NULL;
    if (_state != NULL) {
        state = _state;
    } else {
        // _state 可能大小
        const size_t guess_size = std::max(strlen(fmt) * 2, 32UL);
        const size_t st_size = status_size(guess_size);
        new_state = reinterpret_cast<State*>(malloc(st_size));
        if (NULL == new_state) {
            return -1;
        }
        new_state->state_size = st_size;
        state = new_state;
    }

    // _state->message 容量（错误字符串最大长度）
    const size_t cap = state->state_size - offsetof(State, message);
    va_list copied_args;
    va_copy(copied_args, args);
    // 将可变参数格式化输出到一个字符数组，返回最终生成字符串的长度（不包含终止符）。
    const int bytes_used = vsnprintf(state->message, cap, fmt, copied_args);
    va_end(copied_args);
    if (bytes_used < 0) {
        free(new_state);
        return -1;
    } else if ((size_t)bytes_used < cap) {
        // There was enough room, just shrink and return.
        // 
        // 有足够的空间，只是收缩返回。
        state->code = c;
        state->size = bytes_used;
        if (new_state == state) {
            _state = new_state;
        }
        return 0;
    } else {
        // 重新申请足够大的内存空间
        free(new_state);
        const size_t st_size = status_size(bytes_used);
        new_state = reinterpret_cast<State*>(malloc(st_size));
        if (NULL == new_state) {
            return -1;
        }
        new_state->code = c;
        new_state->size = bytes_used;
        new_state->state_size = st_size;
        // bytes_used + 1 为结尾的 NULL
        const int bytes_used2 =
            vsnprintf(new_state->message, bytes_used + 1, fmt, args);
        if (bytes_used2 != bytes_used) {
            free(new_state);
            return -1;
        }
        free(_state);
        _state = new_state;
        return 0;
    }
}

int Status::set_error(int c, const butil::StringPiece& error_msg) {
    if (0 == c) {
        free(_state);
        _state = NULL;
        return 0;
    }
    const size_t st_size = status_size(error_msg.size());
    if (_state == NULL || _state->state_size < st_size) {
        State* new_state = reinterpret_cast<State*>(malloc(st_size));
        if (NULL == new_state) {
            return -1;
        }
        new_state->state_size = st_size;
        free(_state);
        _state = new_state;
    }
    _state->code = c;
    _state->size = error_msg.size();
    memcpy(_state->message, error_msg.data(), error_msg.size());
    _state->message[error_msg.size()] = '\0';
    return 0;
}

Status::State* Status::copy_state(const State* s) {
    const size_t n = status_size(s->size);
    State* s2 = reinterpret_cast<State*>(malloc(n));
    if (NULL == s2) {
        // TODO: If we failed to allocate, the status will be OK.
        return NULL;
    }
    s2->code = s->code;
    s2->size = s->size;
    s2->state_size = n;
    char* msg_head = s2->message;
    memcpy(msg_head, s->message, s->size);
    msg_head[s->size] = '\0';
    return s2;
};

std::string Status::error_str() const {
    if (_state == NULL) {
        static std::string s_ok_str = "OK";
        return s_ok_str;
    }
    return std::string(_state->message, _state->size);
}

}  // namespace butil

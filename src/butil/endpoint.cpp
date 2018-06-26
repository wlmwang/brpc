// Copyright (c) 2011 Baidu, Inc.
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
// Date: Mon. Nov 7 14:47:36 CST 2011

#include "butil/build_config.h"                // OS_MACOSX
#include <arpa/inet.h>                         // inet_pton, inet_ntop
#include <netdb.h>                             // gethostbyname_r
#include <unistd.h>                            // gethostname
#include <errno.h>                             // errno
#include <string.h>                            // strcpy
#include <stdio.h>                             // snprintf
#include <stdlib.h>                            // strtol
#include "butil/fd_guard.h"                    // fd_guard
#include "butil/endpoint.h"                    // ip_t
#include "butil/logging.h"
#include "butil/memory/singleton_on_pthread_once.h"
#include "butil/strings/string_piece.h"

// connect() 包装函数。
// BAIDU_WEAK 表明该名称为弱符号（可能被强符号替代，如 bthread/fd.cpp 中实现）。
__BEGIN_DECLS
int BAIDU_WEAK bthread_connect(
    int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen) {
    return connect(sockfd, serv_addr, addrlen);
}
__END_DECLS

namespace butil {

// 转换 const char* ip_str 字符串到 ip_t 整型
int str2ip(const char* ip_str, ip_t* ip) {
    // ip_str can be NULL when called by EndPoint(0, ...)
    if (ip_str != NULL) {
        // 去除前导空白
        for (; isspace(*ip_str); ++ip_str);
        int rc = inet_pton(AF_INET, ip_str, ip);
        if (rc > 0) {
            return 0;
        }
    }
    return -1;
}

// 转换 ip_t 整型到 c-style 字符串
IPStr ip2str(ip_t ip) {
    IPStr str;
    if (inet_ntop(AF_INET, &ip, str._buf, INET_ADDRSTRLEN) == NULL) {
        return ip2str(IP_NONE);
    }
    return str;
}

// ip_t 到 hostname 转换。底层使用 getnameinfo(). 会查询 DNS 服务器。
int ip2hostname(ip_t ip, char* host, size_t host_len) {
    if (host == NULL || host_len == 0) {
        errno = EINVAL;
        return -1;
    }
    sockaddr_in sa;
    bzero((char*)&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = 0;    // useless since we don't need server_name
    sa.sin_addr = ip;
    if (getnameinfo((const sockaddr*)&sa, sizeof(sa),
                    host, host_len, NULL, 0, NI_NAMEREQD) != 0) {
        return -1;
    }
    // remove baidu-specific domain name (that every name has)
    // 
    // @todo
    // 去除地址中顶级域名为 .baidu.com 部分的字符串，只取主机名部分。
    butil::StringPiece str(host);
    if (str.ends_with(".baidu.com")) {
        host[str.size() - 10] = '\0';
    }
    return 0;
}

// ip_t 到 hostname 转换。 std::string 版本。
int ip2hostname(ip_t ip, std::string* host) {
    char buf[128];
    if (ip2hostname(ip, buf, sizeof(buf)) == 0) {
        host->assign(buf);
        return 0;
    }
    return -1;
}

EndPointStr endpoint2str(const EndPoint& point) {
    EndPointStr str;
    if (inet_ntop(AF_INET, &point.ip, str._buf, INET_ADDRSTRLEN) == NULL) {
        return endpoint2str(EndPoint(IP_NONE, 0));
    }
    char* buf = str._buf + strlen(str._buf);
    *buf++ = ':';
    // 端口格式化成十进制字符串
    snprintf(buf, 16, "%d", point.port);
    return str;
}

// hostname 到 ip_t 转换。如果 |hostname| 为 NULL ，hostname 取本地主机名。
int hostname2ip(const char* hostname, ip_t* ip) {
    char buf[256];
    if (NULL == hostname) {
        // 获取本机名
        if (gethostname(buf, sizeof(buf)) < 0) {
            return -1;
        }
        hostname = buf;
    } else {
        // skip heading space
        // 
        // 去除前导空白
        for (; isspace(*hostname); ++hostname);
    }

#if defined(OS_MACOSX)
    // gethostbyname on MAC is thread-safe (with current usage) since the
    // returned hostent is TLS. Check following link for the ref:
    // https://lists.apple.com/archives/darwin-dev/2006/May/msg00008.html
    struct hostent* result = gethostbyname(hostname);
    if (result == NULL) {
        return -1;
    }
#else
    char aux_buf[1024];
    int error = 0;
    struct hostent ent;
    struct hostent* result = NULL;
    if (gethostbyname_r(hostname, &ent, aux_buf, sizeof(aux_buf),
                        &result, &error) != 0 || result == NULL) {
        return -1; 
    }
#endif // defined(OS_MACOSX)
    // Only fetch the first address here
    // 
    // 仅取第一个 ip 地址
    bcopy((char*)result->h_addr, (char*)ip, result->h_length);
    return 0;
}

// 本机地址信息
struct MyAddressInfo {
    char my_hostname[256];  // 主机名
    ip_t my_ip;             // 主机地址（网络字节序）
    IPStr my_ip_str;        // 主机地址（c-style 字符串）
    
    MyAddressInfo() {
        my_ip = IP_ANY;
        if (gethostname(my_hostname, sizeof(my_hostname)) < 0) {
            my_hostname[0] = '\0';
        } else if (hostname2ip(my_hostname, &my_ip) != 0) {
            my_ip = IP_ANY;
        }
        my_ip_str = ip2str(my_ip);
    }
};

// 线程安全且永不删除的全局的类型 MyAddressInfo 的单例
ip_t my_ip() {
    return get_leaky_singleton<MyAddressInfo>()->my_ip;
}

const char* my_ip_cstr() {
    return get_leaky_singleton<MyAddressInfo>()->my_ip_str.c_str();
}

const char* my_hostname() {
    return get_leaky_singleton<MyAddressInfo>()->my_hostname;
}

// |str| 格式 "0.0.0.0:0000" 转换为 EndPoint 结构
int str2endpoint(const char* str, EndPoint* point) {
    // Should be enough to hold ip address
    char buf[64];
    size_t i = 0;
    for (; i < sizeof(buf) && str[i] != '\0' && str[i] != ':'; ++i) {
        buf[i] = str[i];
    }
    if (i >= sizeof(buf) || str[i] != ':') {
        return -1;
    }
    buf[i] = '\0';
    if (str2ip(buf, &point->ip) != 0) {
        return -1;
    }
    ++i;
    char* end = NULL;
    point->port = strtol(str + i, &end, 10);
    if (end == str + i) {
        return -1;
    } else if (*end) {
        for (++end; isspace(*end); ++end);
        if (*end) {
            return -1;
        }
    }
    if (point->port < 0 || point->port > 65535) {
        return -1;
    }
    return 0;
}

int str2endpoint(const char* ip_str, int port, EndPoint* point) {
    if (str2ip(ip_str, &point->ip) != 0) {
        return -1;
    }
    if (port < 0 || port > 65535) {
        return -1;
    }
    point->port = port;
    return 0;
}

// |str| 格式 "hostname:0000" 转换为 EndPoint 结构
int hostname2endpoint(const char* str, EndPoint* point) {
    // Should be enough to hold ip address
    char buf[64];
    size_t i = 0;
    for (; i < sizeof(buf) - 1 && str[i] != '\0' && str[i] != ':'; ++i) {
        buf[i] = str[i];
    }
    if (i == sizeof(buf) - 1) {
        return -1;
    }
    
    buf[i] = '\0';
    if (hostname2ip(buf, &point->ip) != 0) {
        return -1;
    }
    if (str[i] == ':') {
        ++i;
    }
    char* end = NULL;
    point->port = strtol(str + i, &end, 10);
    if (end == str + i) {
        return -1;
    } else if (*end) {
        for (; isspace(*end); ++end);
        if (*end) {
            return -1;
        }
    }
    if (point->port < 0 || point->port > 65535) {
        return -1;
    }
    return 0;
}

int hostname2endpoint(const char* name_str, int port, EndPoint* point) {
    if (hostname2ip(name_str, &point->ip) != 0) {
        return -1;
    }
    if (port < 0 || port > 65535) {
        return -1;
    }
    point->port = port;
    return 0;
}

// 转换 EndPoint 结构到 c-style 字符串的 hostname
int endpoint2hostname(const EndPoint& point, char* host, size_t host_len) {
    if (ip2hostname(point.ip, host, host_len) == 0) {
        size_t len = strlen(host);
        if (len + 1 < host_len) {
            snprintf(host + len, host_len - len, ":%d", point.port);
        }
        return 0;
    }
    return -1;
}

int endpoint2hostname(const EndPoint& point, std::string* host) {
    char buf[128];
    if (endpoint2hostname(point, buf, sizeof(buf)) == 0) {
        host->assign(buf);
        return 0;
    }
    return -1;
}

int tcp_connect(EndPoint point, int* self_port) {
    fd_guard sockfd(socket(AF_INET, SOCK_STREAM, 0));
    if (sockfd < 0) {
        return -1;
    }
    struct sockaddr_in serv_addr;
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr = point.ip;
    serv_addr.sin_port = htons(point.port);
    int rc = 0;
    // connect 连接服务器
    if (bthread_connect != NULL) {
        rc = bthread_connect(sockfd, (struct sockaddr*)&serv_addr,
                             sizeof(serv_addr));
    } else {
        rc = ::connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    }
    if (rc < 0) {
        return -1;
    }
    // 将本地绑定的端口写入 |self_port| 参数
    if (self_port != NULL) {
        EndPoint pt;
        if (get_local_side(sockfd, &pt) == 0) {
            *self_port = pt.port;
        } else {
            CHECK(false) << "Fail to get the local port of sockfd=" << sockfd;
        }
    }
    // 连接成功。释放 fd_guard 所有权，返回该 fd 描述符
    return sockfd.release();
}

int tcp_listen(EndPoint point, bool reuse_addr) {
    fd_guard sockfd(socket(AF_INET, SOCK_STREAM, 0));
    if (sockfd < 0) {
        return -1;
    }
    if (reuse_addr) {
        const int on = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                       &on, sizeof(on)) != 0) {
            return -1;
        }
    }
    struct sockaddr_in serv_addr;
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr = point.ip;
    serv_addr.sin_port = htons(point.port); 
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        return -1;
    }
    // 内核会默默地将 backlog 截断到 /proc/sys/net/core/somaxconn 中定义的值
    if (listen(sockfd, INT_MAX) != 0) {
        //             ^^^ kernel would silently truncate backlog to the value
        //             defined in /proc/sys/net/core/somaxconn
        return -1;
    }
    // 监听成功。释放 fd_guard 所有权，返回 fd 描述符
    return sockfd.release();
}

// 获取本端 connect socket 信息
int get_local_side(int fd, EndPoint *out) {
    struct sockaddr addr;
    socklen_t socklen = sizeof(addr);
    const int rc = getsockname(fd, &addr, &socklen);
    if (rc != 0) {
        return rc;
    }
    if (out) {
        *out = butil::EndPoint(*(sockaddr_in*)&addr);
    }
    return 0;
}

// 获取对端 connect socket 信息
int get_remote_side(int fd, EndPoint *out) {
    struct sockaddr addr;
    socklen_t socklen = sizeof(addr);
    const int rc = getpeername(fd, &addr, &socklen);
    if (rc != 0) {
        return rc;
    }
    if (out) {
        *out = butil::EndPoint(*(sockaddr_in*)&addr);
    }
    return 0;
}

}  // namespace butil

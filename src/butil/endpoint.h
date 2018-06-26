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

// Wrappers of IP and port.
// 
// ip/port 包装器

#ifndef BUTIL_ENDPOINT_H
#define BUTIL_ENDPOINT_H

#include <netinet/in.h>                          // in_addr
#include <iostream>                              // std::ostream
#include "butil/containers/hash_tables.h"         // hashing functions


// @tips
// 以下结构可能与具体平台具体实现而声明方式不同。
// 
// \file <sys/socket.h>
// struct sockaddr {
//      // address family, AF_xxx (AF_INET ...) // 协议族，在socket中只能是 AF_INET
//      unsigned short sa_family;
//      // 14 bytes of protocol address // 14 字节协议地址
//      char sa_data[14];
// };
// 此数据结构用做 bind/connect/recvfrom/sendto 等网络库函数的参数，指明地址信息。
// 注意：一般实际 socket 编程中并不直接针对此数据结构操作，而是使用另一个与 sockaddr 等价的
// 数据结构 sockaddr_in 。
// 
// Use like:
// ...
// struct sockaddr_in mysock;
// bzero((char*)&mysock, sizeof(mysock));
// mysock.sin_family = AF_INET;
// mysock.sin_addr.s_addr = ::inet_addr("0.0.0.0");
// ...
// ::bind(sockfd, (struct sockaddr *)&mysock, sizeof(struct sockaddr));
// ...
// ---------------------------------------------------------------------------------------------
// \file <netinet/in.h>
// struct sockaddr_in {
//      // Address family, AF_xxx (AF_INET ...) // 协议族，在socket中只能是 AF_INET
//      short int sin_family;
//      // Port number  // 端口号，使用网络字节顺序(大端)
//      unsigned short int sin_port;
//      // Internet address // IP地址，使用网络字节顺序。具体见 struct in_addr
//      struct in_addr sin_addr;
//      // Same size as struct sockaddr // sockaddr 与 sockaddr_in 两个结构保持相同大小而保留的空字节
//      unsigned char sin_zero[8];
// };
// 
// ---------------------------------------------------------------------------------------------
// \file <netinet/in.h>
// struct in_addr {
//      in_addr_t s_addr;    // 32 位 IP 地址，按照网络字节
// };
// typedef uint32_t in_addr_t;
// 
// ---------------------------------------------------------------------------------------------
// \file <arpa/inet.h>
// int inet_pton(int family, const char *strptr, void *addrptr);
// 将由指针 strptr 所指的 "点分十进制" 转换为 "二进制整数"。存放在 addrptr 所指空间中。
// 如果成功，则返回值为 1 ；如果对于指定的 family 输入串不是有效的表达格式，则返回值为 0 ；若处理失败，函数返
// 回 -1
// 
// Use like:
// struct in_addr s;
// char ipstr[INET_ADDRSTRLEN] = "0.0.0.0";
// if (inet_pton(AF_INET, ipstr, (void *)&s) <= 0) {
//      perror("fail to convert");
//      exit(1); 
// }
// 
// const char* inet_ntop(int family, const void *addrptr, char *strptr, size_t len);
// 将由指针 addrptr 所指的 "二进制整数" 转换为 "点分十进制" 。存放在 strptr 所指的空间中。参数 len 是目标的
// 大小，以免函数溢出其调用者的缓冲区。若函数处理成功，返回指向结果的指针；若函数处理失败，返回 NULL
// 另外为了有助于规定指定目标缓冲区大小，在头文件 <netinet/in.h> 中有如下定义
// #define INET_ADDRSTRLEN 16   // for IPv4 dotted-decimal
// #define INET6_ADDRSTRLEN 46  // for IPv6 hex string
// 
// Use like:
// struct in_addr s;
// char ipstr[INET_ADDRSTRLEN] = "0.0.0.0";
// if (inet_pton(AF_INET, ipstr, (void *)&s) <= 0) {
//      perror("fail to convert");
//      exit(1); 
// }
// 
// if (inet_ntop(AF_INET, (void *)&s, ipstr, INET_ADDRSTRLEN) == NULL) {
//      perror("fail to convert");
//      exit(1);
// }


// @tips
// \file <netdb.h> # 查询 DNS
// int getnameinfo(const struct sockaddr *restrict addr, socklen_t addrlen, char *restrict host, 
//      size_t hostlen, char *restrict serv, size_t servlen, int flags);
// 以一个套接口地址 addr 为参数，返回一个描述主机的字符串 host 和一个描述服务的字符串 serv
// flags:
// NI_MAXHOST  返回的主机字符串的最大长度
// NI_MAXSERV  返回的服务字符串的最大长度
// 
// struct hostent {
//      // official name of host // 主机的规范名 www.google.com
//      char  *h_name;
//      // alias list // 主机的别名
//      char **h_aliases;
//      // host address type // 主机 ip 地址的类型，到底是 ipv4(AF_INET)，还是 ipv6(AF_INET6)
//      int    h_addrtype;
//      // length of address // 主机 ip 地址的长度
//      int    h_length;
//      // list of addresses // 主机的 ip 地址(网络字节序存储)，可能需要调用 inet_ntop() 转换
//      char **h_addr_list;
// }
// #define h_addr h_addr_list[0] /* for backward compatibility */
//
// struct hostent *gethostbyname(const char *name);
// 根据域名或者主机名（例如 "www.google.cn" 等等）转换成是一个 hostent 的结构。如果函数调用失败，将返回 NULL.
// gethostbyname 返回的是一个指向静态变量的指针，不可重入。
// 
// int gethostbyname_r(const char *restrict name, struct hostent *restrict ret, char *restrict buf, 
//      size_t buflen, struct hostent **restrict result, int *restrict h_errnop);
// 是 gethostbyname 的线程安全版本。


// @tips
// \file <netinet/in.h>
// #define INADDR_ANY ((in_addr_t) 0x00000000)
// INADDR_ANY 就是指定地址为 0.0.0.0 的地址，这个地址事实上表示不确定地址、所有地址、任意地址
// 一般来说，在各个系统中均定义成为 0 值。
// 
// #define INADDR_NONE ((in_addr_t) 0xffffffff)
// 代表地址为 255.255.255.255 ，一般来说，表示该地址无效。如在函数 inet_addr() 转换中：如果 
// cp 参数中的字符串不包含合法的 Internet 地址。例如：如果 "a.b.c.d" 地址的一部分超过 255 ，
// 则 inet_addr 将返回值 INADDR_NONE

namespace butil {

// Type of an IP address
// 
// 一般实现中，该 struct 结构里面只有一个 uint32_t 类型的 s_addr 成员。
typedef struct in_addr ip_t;

static const ip_t IP_ANY = { INADDR_ANY };
static const ip_t IP_NONE = { INADDR_NONE };

// Convert |ip| to an integral
// 
// 转换 ip_t 到 in_addr_t 整型(uint32_t)
inline in_addr_t ip2int(ip_t ip) { return ip.s_addr; }

// Convert integral |ip_value| to an IP
// 
// 转换 in_addr_t 整型(uint32_t) 到 ip_t
inline ip_t int2ip(in_addr_t ip_value) {
    const ip_t ip = { ip_value };
    return ip;
}

// Convert string `ip_str' to ip_t *ip.
// `ip_str' is in IPv4 dotted-quad format: `127.0.0.1', `10.23.249.73' ...
// Returns 0 on success, -1 otherwise.
// 
// 转换 const char* ip_str 字符串到 ip_t 结构。底层使用 inet_pton()
// 成功返回 0 ，失败返回 -1
int str2ip(const char* ip_str, ip_t* ip);

// c-style 字符串类型 ip 包装器
struct IPStr {
    const char* c_str() const { return _buf; }
    char _buf[INET_ADDRSTRLEN];
};

// Convert IP to c-style string. Notice that you can serialize ip_t to
// std::ostream directly. Use this function when you don't have streaming log.
// Example: printf("ip=%s\n", ip2str(some_ip).c_str());
// 
// 转换 ip_t 到 c-style 字符串。底层使用 inet_ntop()
IPStr ip2str(ip_t ip);

// Convert `hostname' to ip_t *ip. If `hostname' is NULL, use hostname
// of this machine.
// `hostname' is typically in this form: `tc-cm-et21.tc' `db-cos-dev.db01' ...
// Returns 0 on success, -1 otherwise.
// 
// hostname 到 ip_t 转换。如果 |hostname| 为 NULL ，hostname 取本地主机名。底层使
// 用 gethostbyname_r()
// |hostname| 值形如 `tc-cm-et21.tc' `db-cos-dev.db01' ...
// 成功返回 0 ，失败返回 -1
int hostname2ip(const char* hostname, ip_t* ip);

// Convert `ip' to `hostname'.
// Returns 0 on success, -1 otherwise and errno is set.
// 
// ip_t 到 hostname 转换。底层使用 getnameinfo(). 会查询 DNS 服务器。
// 成功返回 0 ，失败返回 -1
int ip2hostname(ip_t ip, char* hostname, size_t hostname_len);
int ip2hostname(ip_t ip, std::string* hostname);

// Hostname of this machine, "" on error.
// NOTE: This function caches result on first call.
// 
// 获取本机 hostname 。错误返回 '\0'。底层使用 gethostname()
// 注意：该函数会在第一次调用时缓存结果。
const char* my_hostname();

// IP of this machine, IP_ANY on error.
// NOTE: This function caches result on first call.
// 
// 获取本机 ip 整型。错误返回 IP_ANY。底层使用 hostname2ip()
// 注意：该函数会在第一次调用时缓存结果。
ip_t my_ip();
// String form.
// 
// 本地 ip 地址 c-style 字符串形式
const char* my_ip_cstr();

// ipv4 + port
struct EndPoint {
    EndPoint() : ip(IP_ANY), port(0) {}
    EndPoint(ip_t ip2, int port2) : ip(ip2), port(port2) {}
    explicit EndPoint(const sockaddr_in& in)
        : ip(in.sin_addr), port(ntohs(in.sin_port)) {}
    
    ip_t ip;
    int port;
};

// @todo
// 16字节端口号？
// EndPoint 字符串形式。形如 0.0.0.0:0000
struct EndPointStr {
    const char* c_str() const { return _buf; }
    char _buf[INET_ADDRSTRLEN + 16];
};

// Convert EndPoint to c-style string. Notice that you can serialize 
// EndPoint to std::ostream directly. Use this function when you don't 
// have streaming log.
// Example: printf("point=%s\n", endpoint2str(point).c_str());
// 
// 将 EndPoint 转换为 c-style 字符串。可以直接将 EndPoint 序列化为 std::ostream。
// 例如： printf("point=%s\n", endpoint2str(point).c_str());
EndPointStr endpoint2str(const EndPoint&);

// Convert string `ip_and_port_str' to a EndPoint *point.
// Returns 0 on success, -1 otherwise.
// 
// 转换 c-style 字符串 |ip_and_port_str| 到 EndPoint 结构。
// 成功返回 0 ，失败返回 -1
int str2endpoint(const char* ip_and_port_str, EndPoint* point);
int str2endpoint(const char* ip_str, int port, EndPoint* point);

// Convert `hostname_and_port_str' to a EndPoint *point.
// Returns 0 on success, -1 otherwise.
// 
// 转换 c-style 字符串 |ip_and_port_str| 到 EndPoint 结构。
// 成功返回 0 ，失败返回 -1
int hostname2endpoint(const char* ip_and_port_str, EndPoint* point);
int hostname2endpoint(const char* name_str, int port, EndPoint* point);

// Convert `endpoint' to `hostname'.
// Returns 0 on success, -1 otherwise and errno is set.
// 
// 转换 EndPoint 结构到 c-style 字符串的 hostname
// // 成功返回 0 ，失败返回 -1
int endpoint2hostname(const EndPoint& point, char* hostname, size_t hostname_len);
int endpoint2hostname(const EndPoint& point, std::string* host);

// Create a TCP socket and connect it to `server'. Write port of this side
// into `self_port' if it's not NULL.
// Returns the socket descriptor, -1 otherwise and errno is set.
// 
// 创建一个 TCP socket 并 connect() 连接到 |server| 服务器上。成功，并且 |self_port| 不
// 为 NULL 时，将本地绑定的端口写入 |self_port| 参数。
// 成功返回该 connect socket 描述符，失败返回 -1 ，并设置 errno
int tcp_connect(EndPoint server, int* self_port);

// Create and listen to a TCP socket bound with `ip_and_port'. If `reuse_addr'
// is true, ports in TIME_WAIT will be bound as well.
// Returns the socket descriptor, -1 otherwise and errno is set.
// 
// 创建一个 TCP socket 且 bind() 到 |ip_and_port| 上后 listen 监听。如果 |reuse_addr|
// 为 true (SO_REUSEADDR), 端口在 TIME_WAIT 状态也可被重用。
// 成功返回该 listen socket 描述符，失败返回 -1 ，并设置 errno
int tcp_listen(EndPoint ip_and_port, bool reuse_addr);

// Get the local end of a socket connection
// 
// 获取本端 connect socket 信息（本端、对端。 tcp socket 为全双工通信协议）
int get_local_side(int fd, EndPoint *out);

// Get the other end of a socket connection
// 
// 获取对端 connect socket 信息（本端、对端。 tcp socket 为全双工通信协议）
int get_remote_side(int fd, EndPoint *out);

}  // namespace butil

// Since ip_t is defined from in_addr which is globally defined, due to ADL
// we have to put overloaded operators globally as well.
// 
// @todo
// 重载定义 butil::ip_t 类型的关系运算符。
inline bool operator<(butil::ip_t lhs, butil::ip_t rhs) {
    return butil::ip2int(lhs) < butil::ip2int(rhs);
}
inline bool operator>(butil::ip_t lhs, butil::ip_t rhs) {
    return rhs < lhs;
}
inline bool operator>=(butil::ip_t lhs, butil::ip_t rhs) {
    return !(lhs < rhs);
}
inline bool operator<=(butil::ip_t lhs, butil::ip_t rhs) {
    return !(rhs < lhs); 
}
inline bool operator==(butil::ip_t lhs, butil::ip_t rhs) {
    return butil::ip2int(lhs) == butil::ip2int(rhs);
}
inline bool operator!=(butil::ip_t lhs, butil::ip_t rhs) {
    return !(lhs == rhs);
}

// 重载定义 butil::IPStr/butil::ip_t 类型的输出运算符。
inline std::ostream& operator<<(std::ostream& os, const butil::IPStr& ip_str) {
    return os << ip_str.c_str();
}
inline std::ostream& operator<<(std::ostream& os, butil::ip_t ip) {
    return os << butil::ip2str(ip);
}

namespace butil {
// Overload operators for EndPoint in the same namespace due to ADL.
// 
// @todo
// 在同一命名空间中，重载 EndPoint 类型的关系运算符。
inline bool operator<(EndPoint p1, EndPoint p2) {
    return (p1.ip != p2.ip) ? (p1.ip < p2.ip) : (p1.port < p2.port);
}
inline bool operator>(EndPoint p1, EndPoint p2) {
    return p2 < p1;
}
inline bool operator<=(EndPoint p1, EndPoint p2) { 
    return !(p2 < p1); 
}
inline bool operator>=(EndPoint p1, EndPoint p2) { 
    return !(p1 < p2); 
}
inline bool operator==(EndPoint p1, EndPoint p2) {
    return p1.ip == p2.ip && p1.port == p2.port;
}
inline bool operator!=(EndPoint p1, EndPoint p2) {
    return !(p1 == p2);
}

// 重载定义 butil::EndPoint/butil::EndPointStr 类型的输出运算符。
inline std::ostream& operator<<(std::ostream& os, const EndPoint& ep) {
    return os << ep.ip << ':' << ep.port;
}
inline std::ostream& operator<<(std::ostream& os, const EndPointStr& ep_str) {
    return os << ep_str.c_str();
}

}  // namespace butil


namespace BUTIL_HASH_NAMESPACE {

// Implement methods for hashing a pair of integers, so they can be used as
// keys in STL containers.
// 
// 实现 butil::EndPoint 的 hash 散列函数，让他们可以用作 STL 容器中的键。

#if defined(COMPILER_MSVC)

inline std::size_t hash_value(const butil::EndPoint& ep) {
    return butil::HashPair(butil::ip2int(ep.ip), ep.port);
}

#elif defined(COMPILER_GCC)
template <>
struct hash<butil::EndPoint> {
    std::size_t operator()(const butil::EndPoint& ep) const {
        return butil::HashPair(butil::ip2int(ep.ip), ep.port);
    }
};

#else
#error define hash<EndPoint> for your compiler
#endif  // COMPILER

}

#endif  // BUTIL_ENDPOINT_H

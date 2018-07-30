// Copyright (c) 2014 Baidu, Inc.
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

// Author: Jiang,Rujie(jiangrujie@baidu.com)  
// Date: Mon. Jan 27  23:08:35 CST 2014

#include <sys/types.h>                          // socket
#include <sys/socket.h>                         // ^
#include <sys/un.h>                             // unix domain socket
#include "butil/fd_guard.h"                     // fd_guard
#include "butil/logging.h"

namespace butil {

int unix_socket_listen(const char* sockname, bool remove_previous_file) {
    struct sockaddr_un addr;
    addr.sun_family = AF_LOCAL;
    // listen socket 路径地址
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sockname);

    // 创建 listen socket 文件描述符
    fd_guard fd(socket(AF_LOCAL, SOCK_STREAM, 0));
    if (fd < 0) {
        PLOG(ERROR) << "Fail to create unix socket";
        return -1;
    }
    if (remove_previous_file) {
        remove(sockname);
    }
    // 绑定地址到描述符上
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        PLOG(ERROR) << "Fail to bind sockfd=" << fd << " as unix socket="
                    << sockname;
        return -1;
    }
    // 监听该 socket 描述符
    if (listen(fd, SOMAXCONN) != 0) {
        PLOG(ERROR) << "Fail to listen to sockfd=" << fd;
        return -1;
    }
    // 释放 fd 所有权，并返回 fd
    return fd.release();
}

int unix_socket_listen(const char* sockname) {
    return unix_socket_listen(sockname, true);
}

int unix_socket_connect(const char* sockname) {
    struct sockaddr_un addr;
    addr.sun_family = AF_LOCAL;
    // listen socket 路径地址
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sockname);

    // 创建 connect socket 文件描述符
    fd_guard fd(socket(AF_LOCAL, SOCK_STREAM, 0));
    if (fd < 0) {
        PLOG(ERROR) << "Fail to create unix socket";
        return -1;
    }
    // 连接 listen socket 地址
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        PLOG(ERROR) << "Fail to connect to unix socket=" << sockname
                    << " via sockfd=" << fd;
        return -1;
    }
    // 释放 fd 所有权，并返回 fd
    return fd.release();
}

}  // namespace butil

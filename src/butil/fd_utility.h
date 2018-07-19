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

// Utility functions on file descriptor.
// 
// 处理文件描述符的实用工具函数

#ifndef BUTIL_FD_UTILITY_H
#define BUTIL_FD_UTILITY_H

namespace butil {

// Make file descriptor |fd| non-blocking
// Returns 0 on success, -1 otherwise and errno is set (by fcntl)
// 
// 使用 fcntl 设置文件描述符 fd 为非阻塞标志。
// 成功返回 0，失败返回 -1，并设置 errno
int make_non_blocking(int fd);

// Make file descriptor |fd| blocking
// Returns 0 on success, -1 otherwise and errno is set (by fcntl)
// 
// 使用 fcntl 设置文件描述符 fd 为阻塞标志。
// 成功返回0，失败返回 -1，并设置 errno
int make_blocking(int fd);

// Make file descriptor |fd| automatically closed during exec()
// Returns 0 on success, -1 when error and errno is set (by fcntl)
// 
// 使用 fcntl 设置文件描述符 fd 在进程调用 exec() 克隆时，自动关闭。
// 成功返回 0，失败返回 -1，并设置 errno
int make_close_on_exec(int fd);


// @tips
// Nagle's Algorithm 是为了提高带宽利用率而设计的算法，其做法是合并小的 TCP 包为
// 一个，避免了过多的小报文的 TCP 头所浪费的带宽。如果开启了这个算法（默认），则协
// 议栈会累积数据直到以下两个条件之一满足的时候才真正发送出去：
// 1. 积累的数据量到达最大的 TCP Segment Size
// 2. 收到了一个 Ack
// 
// TCP Delayed Acknoledgement 也是为了类似的目的被设计出来的，它的作用就是延迟 
// Ack 包的发送，使得协议栈有机会合并多个 Ack ，提高网络性能。
// 
// 描述场景：
// 如果一个 TCP 连接的一端启用了 Nagle's Algorithm，而另一端启用了 TCP Delayed Ack，
// 而发送的数据包又比较小，则可能会出现这样的情况：
// 1. 发送端在等待接收端对上一个 packet 的 Ack 才发送当前的 packet，而接收端则正好延迟
// 	  了此 Ack 的发送，那么这个正要被发送的 packet 就会同样被延迟。
// 2. 当然 Delayed Ack 是有个超时机制的，而默认的超时是 40ms 。
// 
// 
// 现代 TCP/IP 协议栈的实现，默认几乎都启用了这两个功能，你可能会想，按上面的说法，当协议
// 报文很小的时候，岂不每次都会触发这个延迟问题？
// 
// #### 只有 write-write-read 模式，才可能会出问题！		
// 事实上仅当协议的交互是发送端连续发送两个 packet ，然后立刻 read 的时候才会出现问题。
// 
// 详解如下：
// 下面是来自维基百科 Nagle’s Algorithm 的伪码
// if there is new data to send
//   if the window size >= MSS and available data is >= MSS
//     send complete MSS segment now
//   else
//     if there is unconfirmed data still in the pipe
//       enqueue data in the buffer until an acknowledge is received
//     else
//       send data immediately
//     end if
//   end if
// end if
// 
// 可以看到，当待发送的数据比 MSS 小的时候（外层的 else 分支），还要再判断当前是否有未确认 
// (ack) 的数据。只有当管道里还有未确认数据的时候才会进入缓冲区，等待 Ack。也就是说发送端发
// 送的第一个 write 是不会被缓冲起来，而是立刻发送的（进入内层的 else 分支），这时接收端收
// 到对应的数据，但它还期待更多数据才进行处理，所以不会往回发送数据，因此也没机会把 Ack 给带
// 回去。而根据 Delayed Ack 机制，这个 Ack 也会被 Hold 住。这时发送端发送第二个包，而队
// 列里还有未确认的数据包，所以进入了内层 if 的 then 分支，这个 packet 会被缓冲起来。此时，
// 发送端在等待接收端的 Ack ；接收端则在 Delay 这个 Ack ，所以都在等待，直到接收端 Deplayed 
// Ack 超时（40ms），此 Ack 被发送回去，发送端缓冲的这个 packet 才会被真正送到接收端，从
// 而继续下去。
// 
// #### 但如果是 write-read-write-read 模式，就没有以上问题！
// 即，接收端已经得到所有需要的数据以进行下一步处理。接收端此时处理完后发送结果，同时也就可以
// 把上一个 packet 的 Ack 可以和数据一起发送回去，不需要 delay ，从而不会导致任何问题。
// 
// 当然，若我们直接关闭 TCP Delayed Acknoledgement 特性，就可达到不关心以上细则，也能解
// 决潜在的 tcp 发送延迟。


// Disable nagling on file descriptor |socket|.
// Returns 0 on success, -1 when error and errno is set (by setsockopt)
// 
// 使用 setsockopt 设置文件描述符 |socket| 禁用 Nagle's Algorithm 。
// 成功返回 0，失败返回 -1，并设置 errno
// 
// 对于既要求低延时，又有大量小数据传输，还同时想提高网络利用率该设置较为理想（也可改用 UDP）
int make_no_delay(int socket);

}  // namespace butil

#endif  // BUTIL_FD_UTILITY_H

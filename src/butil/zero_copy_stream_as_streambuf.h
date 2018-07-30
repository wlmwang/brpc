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
// Date: Thu Nov 22 13:57:56 CST 2012

#ifndef BUTIL_ZERO_COPY_STREAM_AS_STREAMBUF_H
#define BUTIL_ZERO_COPY_STREAM_AS_STREAMBUF_H

#include <streambuf>
#include <google/protobuf/io/zero_copy_stream.h>

namespace butil {

// @tips
// 为了提供更灵活的输入输出控制，并让其支持更多的类型和格式，C++ 引入了输入输出流。C++ 
// 的输入输出系统中提供了两个基类，分别是 ios_base 和 ios。基于这两个基类实现了我们常
// 用的标准输入输出流 istream 和 ostream。同时，基于这两个流，C++ 提供了另外两种类型：
// 文件输入输出流 fstream 以及字符串输入输出流 stringstream。
// 大多数人使用系统提供的输入输出框架即可。但如果你希望在你的项目中像标准输入输出一样来读
// 取 tcp socket，或者希望像标准输入输出一样来封装一个对于 FILE* 的读取和写入，再或者
// 你希望利用输入输出流的方式来操纵内存中的数据时，就需要通过定制流来实现了。
// 每一个输入、输出流都会绑定相应的 buffer ，也就是输入输出缓冲区。这个缓冲区就是基于 
// streambuf 类来定义的。文件输入输出流使用的是继承自 streambuf 的 filebuf，而字符
// 串流则是使用的 stringbuf 。事实上 streambuf 是输入输出系统中最关键的一部分，它负
// 责提供缓冲功能，并提供”顺序读取设备”的抽象，也就是把数据刷新到外部设备中或者从外部设备
// 中读取数据。而具体的流只需要负责进行格式化或者完成其他类型地工作即可。
// 
// streambuf 可以看作一块缓冲区，用来存储数据。在文件流这种情况下，streambuf 是为了避
// 免大量的 IO 操作。在字符串流的情况下，streambuf 是为了提供字符串的格式化读取和输出操
// 作（想象字符串是你从键盘输入的数据）。
// 
// sreambuf 内部维护着六个指针 eback,gptr,egptr;pbase,pptr,epptr; 分别指向读取缓
// 冲区的头、当前读取位置、尾；写缓冲区的头、当前写位置、尾（实际上这几个指针指向同一段缓冲
// 区）。streambuf 本身是个虚基类，不能实例化，要使用 streambuf 就需要继承 streambuf 
// 实现新类。
// 
// 一、 streambuf 定义的输出相关的函数主要有 sputc 和 sputn，前者输出一个字符到缓冲区，
// 并且将指针 pptr 向后移动一个字符，后者调用函数 xsputn 连续输出多个字符，xsputn 默认
// 的实现就是多次调用 sputc。由于缓冲区有限，当 pptr 指针向后移动满足 pptr() == epptr 
// 时，说明缓冲区满了，这时将会调用函数 overflow 将数据写入到外部设备并清空缓冲区；清空缓
// 冲区的方式则是调用 pbump 函数将指针 pptr 重置。注意：由于调用 overflow 时，当前的缓
// 冲区已经满了，因此 overflow 的参数 c 必须在缓冲区中的数据刷新到外部设备之后，才能够放
// 入到 buffer 中，否则 overflow 应该返回 eof 。
// 
// 二、 streambuf 类定义了如下几个函数来支持对于输入缓冲区的读取和管理：
// 1. sgetc: 从输入缓冲区中读取一个字符；
// 2. sbumpc: 从输入缓冲区中读取一个字符，并将 gptr() 指针向后移动一个位置；
// 3. sgetn: 从输入缓冲区中读取 n 个字符；
// 4. sungetc: 将缓冲区的 gptr() 指针向前移动一个位置；
// 5. sputbackc: 将一个读取到的字符重新放回到输入缓冲区中；
// 与输出缓冲区不同的是，输入缓冲区需要额外提供 putback 操作，也就是将字符放回到输入缓冲
// 区内。
// 
// 当输入缓冲区满足 gptr() == egptr() 时，表明缓冲区已经没有数据可以读取，函数 sgetc 
// 将会调用 underflow 函数来从外部设备中拉取数据。不同于 sgetc, sbumpc 在这种情况下则
// 会调用 uflow 来实现拉取数据，并移动缓冲区读取指针的目的。默认情况下， uflow 会调用 
// underflow ，我们也无需额外实现 uflow ，但在特殊情况下（例如没有定义缓冲空间），则需
// 要覆盖实现两个函数。
// 
// 
// Example like:
// #include
// class TcpStreamBuf : public std::streambuf {
//  public:
//      TcpStreamBuf(int socket, size_t buf_size):
//      buf_size_(buf_size), socket_(socket) {
//          assert(buf_size_ > 0);
//          
//          pbuf_ = new char[buf_size_];
//          setp(pbuf_, pbuf_ + buf_size_); // set put pointers for output
//          
//          gbuf_ = new char[buf_size_];
//          setg(gbuf_, gbuf_, gbuf_);  // set get pointers for input
//      }
//      ~TcpStreamBuf() {
//          // buf_size_ = 0;
//          // setp(NULL, NULL);
//          // setg(NULL, NULL, NULL);
//          delete [] pbuf_;
//          delete [] gbuf_;
//      }
//      // 字符 c 是调用 overflow 时当前的字符
//      int overflow(int c) {
//          if (-1 == sync()) {
//              return traits_type::eof();
//          } else {
//              // put c into buffer after successful sync
//              if (!traits_type::eq_int_type(c, traits_type::eof())) {
//                  sputc(traits_type::to_char_type(c));
//              }
//              
//              // return eq_int_type(c, eof()) ? eof(): c;
//              return traits_type::not_eof(c);
//          }
//      }
//      // 将buffer中的内容刷新到外部设备，不管缓冲区是否满
//      int sync() {
//          int sent = 0;
//          int total = pptr() - pbase();  // data that can be flushed
//          while (sent < total) {
//              int ret = send(socket_, pbase()+sent, total-sent, 0);
//              if (ret > 0) {
//                  sent += ret;
//              } else {
//                  return -1;
//              }
//          }
//          
//          setp(pbase(), pbase() + buf_size_);  // reset the buffer
//          pbump(0);  // reset pptr to buffer head
//          
//          return 0;
//      }
//      // 当缓冲区已经没有数据时需要进行的操作，从 socket 中读取数据到 gbuf_ 中，然
//      // 后设置尾指针为 eback() + ret;设置 gptr 为 指向数据的第一个字节 eback。
//      // 同时返回当前可以读取的位置上的数据 *gptr()
//      int underflow() {
//          int ret = recv(socket_, eback(), buf_size_, 0);
//          if (ret > 0) {
//              setg(eback(), eback(), eback() + ret);
//              return traits_type::to_int_type(*gptr());
//          } else {
//              return traits_type::eof();
//          }
//      }
//  private:
//      const size_t buf_size_;
//      int socket_;
//      char* pbuf_;  // 输出缓冲区
//      char* gbuf_;  // 输入缓冲区
//  };
// 
// 
// 三、定义一个 stream 来使用 TcpStreamBuf:
// class BasicTcpStream : public std::iostream {
// public:
//     BasicTcpStream(int socket, size_t buf_size): 
//         iostream(new TcpStreamBuf(socket, buf_size), 
//         socket_(socket), buf_size_(buf_size) {}
//         
//     ~BasicTcpStream() {
//          delete rdbuf();
//     }
// 
// private:
//     int socket_;
//     const size_t buf_size_;
// };
// 
// @see https://github.com/kaiywen/code-for-blog


// @tips
// 我们在序列化、反序列化 Protobuf message 时为了最小化内存拷贝，可以实现其提供的 
// ZeroCopyStream（包括 ZeroCopyOutputStream 和 ZeroCopyInputStream）接口
// 类。 ZeroCopyStream 要求能够进行 buffer 的分配，这体现在一个名为 Next 的接口
// 上，这样做的好处是避免进行内存的拷贝。
// 
// 一、传统的 stream 和 ZeroCopyInputStream 的对比：
// 1. 传统的做法，我们调用 input stream 的 Read() 从内存中读取数据到 buffer 。
// 这里进行了一次拷贝操作，也就是拷贝内存中的数据到 buffer 中。之后 DoSomething() 
// 才能处理此数据。
// Use like:
// char buffer[BUFFER_SIZE];
// input->Read(buffer, BUFFER_SIZE);
// DoSomething(buffer, BUFFER_SIZE);
// 
// 2. 使用 Next() 接口的做法，input stream 内部有责任提供（分配）buffer 。也就
// 是说 DoSomething() 可以直接操作内部的内存，而无需拷贝后再操作。这就避免了一次内
// 存拷贝。
// Use like:
// const void* buffer; int size;
// input->Next(&buffer, &size);
// DoSomething(buffer, size);
// 
// 
// 二、 ZeroCopyOutputStream 接口类有以下几个接口需要实现：
// 
// 1. 获得（分配）一个用于写入数据的 Buffer 给调用者。
// virtual bool Next(void ** data, int * size) = 0;
// 
// 2. 由于调用 Next() 请求， stream 分配了一块 Buffer 给调用者使用，在最后一次 
// Next() 调用时可能分配的 Buffer 大小大于需要的大小， BackUp 接口用于归还多余的
// 内存。
// virtual void BackUp(int count) = 0;
// 
// 3. 返回总的被写入的字节数
// virtual int64 ByteCount() const = 0;
// 
// 注： Next() 接受两个参数 |data| 和 |size|，|data| 和 |size| 保证不为 NULL，
// |data| 用于获取 Buffer 的地址，|size| 用于获取 Buffer 的长度。此函数被调用，意
// 在获取一个可以用来写入数据的连续的 Buffer 。函数返回 false 表示调用失败。
// BackUp() 的意义在于，调用了 Next() 之后获取到一块大小为 |size| 的 Buffer 用于
// 写入数据，但有可能出现数据全部写入完了 Buffer 还未使用完，这时候需要调用 BackUp() 
// 把未使用的内存归还。


// Wrap a ZeroCopyOutputStream into std::streambuf. Notice that before 
// destruction or shrink(), BackUp() of the stream are not called. In another
// word, if the stream is wrapped from IOBuf, the IOBuf may be larger than 
// appended data.
// 
// 将 ZeroCopyOutputStream 包装到 std::streambuf 中。请注意，在销毁或 shrink() 收
// 缩之前， stream 的 BackUp() 不被调用。换句话说，如果 stream 是从 IOBuf 包装的，则 
// IOBuf 可能大于附加数据。
class ZeroCopyStreamAsStreamBuf : public std::streambuf {
public:
    ZeroCopyStreamAsStreamBuf(google::protobuf::io::ZeroCopyOutputStream* stream)
        : _zero_copy_stream(stream) {}
    virtual ~ZeroCopyStreamAsStreamBuf();

    // BackUp() unused bytes. Automatically called in destructor.
    // 
    // BackUp() 未使用的字节。在析构函数中自动调用。
    void shrink();
    
protected:
    // 缓冲区写满。字符 ch 是调用 overflow 时当前的字符
    virtual int overflow(int ch);
    // 将缓冲区中的内容刷新到外部设备，不管缓冲区是否满
    virtual int sync();
    // 根据相对位置移动内部指针
    std::streampos seekoff(std::streamoff off,
                           std::ios_base::seekdir way,
                           std::ios_base::openmode which);

private:
    google::protobuf::io::ZeroCopyOutputStream* _zero_copy_stream;
};

}  // namespace butil

#endif  // BUTIL_ZERO_COPY_STREAM_AS_STREAMBUF_H

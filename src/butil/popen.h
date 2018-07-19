// Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
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

// Author: Zhangyi Chen (chenzhangyi01@baidu.com)
// Date: 2017/11/04 17:13:18

#ifndef  PUBLIC_COMMON_POPEN_H
#define  PUBLIC_COMMON_POPEN_H

#include <ostream>

namespace butil {

// Read the stdout of child process executing `cmd'.
// Returns the exit status(0-255) of cmd and all the output is stored in
// |os|. -1 otherwise and errno is set appropriately.
// 
// 在子进程中执行 `cmd'(shell) ，返回 cmd 的退出状态（0-255），所有输出写入 |os| 
// 中。否则为 -1 ，并且正确设置 errno 。
int read_command_output(std::ostream& os, const char* cmd);

}

#endif  //PUBLIC_COMMON_POPEN_H

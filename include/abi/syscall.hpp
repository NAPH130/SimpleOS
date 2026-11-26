#pragma once

#include <stdint.h>

namespace abi::syscall {

// int 0x80 使用 RAX 传调用号和返回值，RDI、RSI、RDX 传前三个参数。
inline constexpr uint64_t write = 1;
inline constexpr uint64_t exit = 2;
inline constexpr int console_fd = 1; // 当前唯一的输出目标，对应串口终端。

// 返回值使用有符号整数。负值是内核和用户程序共同解释的错误码。
inline constexpr intptr_t permission_denied = -1;
inline constexpr intptr_t bad_file_descriptor = -9;
inline constexpr intptr_t bad_address = -14;
inline constexpr intptr_t invalid_argument = -22;
inline constexpr intptr_t not_implemented = -38;

}

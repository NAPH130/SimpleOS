#pragma once

#include <stddef.h>
#include <stdint.h>

namespace user {

// 将 size 个字节写入终端，返回实际写入字节数或负错误码。当前终端的 fd 为 1。
[[nodiscard]] intptr_t sys_write(int fd, const void* buffer, size_t size);

// 结束当前用户程序，将退出状态交给内核。
[[noreturn]] void sys_exit(int status);

// TODO: 通过系统调用主动让出 CPU，等待调度器恢复执行。
void sys_yield();

}

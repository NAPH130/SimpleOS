#pragma once

#include <stddef.h>
#include <stdint.h>

namespace user {

// TODO: 通过系统调用输出缓冲区，返回实际写入字节数或负错误码。
[[nodiscard]] intptr_t sys_write(int fd, const void* buffer, size_t size);

// TODO: 通过系统调用退出当前用户进程。
[[noreturn]] void sys_exit(int status);

// TODO: 通过系统调用主动让出 CPU，等待调度器恢复执行。
void sys_yield();

}

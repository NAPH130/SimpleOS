#include <abi/syscall.hpp>
#include <arch/x86_64/vectors.h>
#include <user/syscall.hpp>

namespace {

intptr_t memory_call(uintptr_t number, uintptr_t argument) {
    asm volatile("int %1" : "+a"(number) : "i"(X86_64_VECTOR_SYSCALL), "D"(argument) : "memory", "cc");
    return static_cast<intptr_t>(number);
}

}

intptr_t user::sys_write(int fd, const void* buffer, size_t size) {
    uintptr_t result = abi::syscall::write;
    // +a 表示 RAX 既传入调用号又接收结果。D、S 和 d 固定三个参数寄存器。
    // 陷入内核后可能读取 buffer，memory 约束保持缓冲区写入发生在调用之前。
    asm volatile("int %1" : "+a"(result) : "i"(X86_64_VECTOR_SYSCALL),
                 "D"(static_cast<uintptr_t>(fd)), "S"(buffer), "d"(size) : "memory", "cc");
    return static_cast<intptr_t>(result);
}

[[noreturn]] void user::sys_exit(int status) {
    // 正常退出会回到内核启动点。若误返回用户态，立即触发诊断异常。
    asm volatile("int %0\n\tud2" : : "i"(X86_64_VECTOR_SYSCALL),
                 "a"(abi::syscall::exit), "D"(static_cast<uintptr_t>(status)) : "memory", "cc");
    __builtin_unreachable();
}

void* user::sys_alloc(size_t bytes) {
    const intptr_t result = memory_call(abi::syscall::allocate, bytes);
    return result > 0 ? reinterpret_cast<void*>(result) : nullptr;
}

intptr_t user::sys_free(void* address) {
    return memory_call(abi::syscall::release, reinterpret_cast<uintptr_t>(address));
}

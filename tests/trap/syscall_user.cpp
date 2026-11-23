#include <abi/syscall.hpp>
#include <arch/x86_64/paging.hpp>
#include <arch/x86_64/vectors.h>
#include <user/syscall.hpp>

namespace {

void check(bool passed, int status) {
    if (!passed) {
        user::sys_exit(status);
    }
}

}

extern "C" [[noreturn, gnu::section(".text.entry")]] void syscall_user_main() {
    namespace abi_call = abi::syscall;
    constexpr int fd = abi_call::console_fd;
    constexpr size_t first_size = sizeof("Ring 3 wrote 100%\r\n") - 1;
    check(user::sys_write(fd, "Ring 3 wrote 100%\r\nunused", first_size) == first_size, 1);

    check(user::sys_write(2, "X", 1) == abi_call::bad_file_descriptor, 2);
    check(user::sys_write(fd, nullptr, 0) == 0, 3);
    check(user::sys_write(fd, reinterpret_cast<const void*>(0x1000), 1) == abi_call::bad_address, 4);
    check(user::sys_write(fd, reinterpret_cast<const void*>(0xffffffff80000000), 1)
          == abi_call::bad_address, 5);
    check(user::sys_write(fd, "X", SIZE_MAX) == abi_call::invalid_argument, 6);

    uintptr_t rsp;
    asm volatile("mov %%rsp, %0" : "=r"(rsp));
    constexpr uintptr_t page_size = x86_64::paging::page_size;
    auto* stack_end = reinterpret_cast<char*>(rsp | (page_size - 1));
    *stack_end = 'X';
    // 末字节之后是未映射页。整次写入应失败，串口也应看不到 X。
    check(user::sys_write(fd, stack_end, 2) == abi_call::bad_address, 7);

    uintptr_t unknown = 0;
    asm volatile("int %1" : "+a"(unknown) : "i"(X86_64_VECTOR_SYSCALL) : "memory", "cc");
    check(static_cast<intptr_t>(unknown) == abi_call::not_implemented, 8);

    constexpr char resumed[] = "Ring 3 resumed after sys_write\r\n";
    check(user::sys_write(fd, resumed, sizeof(resumed) - 1) == sizeof(resumed) - 1, 9);
    user::sys_exit(42);
}

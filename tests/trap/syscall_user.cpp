#include <abi/syscall.hpp>
#include <arch/x86_64/paging.hpp>
#include <arch/x86_64/vectors.h>
#include <user/syscall.hpp>

namespace {

// 非零退出状态用来指明哪一步失败。内核自检只接受成功路径的状态 42。
void check(bool passed, int status) {
    if (!passed) {
        user::sys_exit(status);
    }
}

}

extern "C" [[noreturn, gnu::section(".text.entry")]] void syscall_user_main() {
    namespace abi_call = abi::syscall;
    constexpr int fd = abi_call::console_fd;
    constexpr uintptr_t page_size = x86_64::paging::page_size;
    uintptr_t rsp;
    asm volatile("mov %%rsp, %0" : "=r"(rsp));
    const uint64_t marker = *reinterpret_cast<const volatile uint64_t*>(rsp & ~(page_size - 1));
    if (marker == 0x42) {
        constexpr char ready[] = "Process B read its own stack data\r\n";
        check(user::sys_write(fd, ready, sizeof(ready) - 1) == sizeof(ready) - 1, 10);
        check(user::sys_alloc(2 * page_size) != nullptr, 11);
        // 保留申请的内存，并用真正的用户态访存触发 #PF，由内核统一回收。
        constexpr uintptr_t unmapped = 0x1000;
        asm volatile("mov (%0), %%rax" : : "r"(unmapped) : "rax", "memory");
        user::sys_exit(12);
    }
    check(marker == 0x41, 13);
    constexpr size_t first_size = sizeof("Ring 3 wrote 100%\r\n") - 1;
    check(user::sys_write(fd, "Ring 3 wrote 100%\r\nunused", first_size) == first_size, 1);

    check(user::sys_write(2, "X", 1) == abi_call::bad_file_descriptor, 2);
    check(user::sys_write(fd, nullptr, 0) == 0, 3);
    check(user::sys_write(fd, reinterpret_cast<const void*>(0x1000), 1) == abi_call::bad_address, 4);
    check(user::sys_write(fd, reinterpret_cast<const void*>(0xffffffff80000000), 1)
          == abi_call::bad_address, 5);
    check(user::sys_write(fd, "X", SIZE_MAX) == abi_call::invalid_argument, 6);

    auto* stack_end = reinterpret_cast<char*>(rsp | (page_size - 1));
    *stack_end = 'X';
    // 末字节之后是未映射页。整次写入应失败，串口也应看不到 X。
    check(user::sys_write(fd, stack_end, 2) == abi_call::bad_address, 7);

    uintptr_t unknown = 0;
    asm volatile("int %1" : "+a"(unknown) : "i"(X86_64_VECTOR_SYSCALL) : "memory", "cc");
    check(static_cast<intptr_t>(unknown) == abi_call::not_implemented, 8);

    constexpr char resumed[] = "Ring 3 resumed after sys_write\r\n";
    check(user::sys_write(fd, resumed, sizeof(resumed) - 1) == sizeof(resumed) - 1, 9);

    check(user::sys_alloc(0) == nullptr && user::sys_alloc(SIZE_MAX) == nullptr, 14);
    check(user::sys_free(reinterpret_cast<void*>(rsp & ~(page_size - 1))) == abi_call::invalid_argument,
          15);
    auto* bytes = static_cast<volatile char*>(user::sys_alloc(page_size + 1));
    check(bytes != nullptr, 16);
    bytes[page_size - 1] = 'A';
    bytes[page_size] = 'B';
    check(bytes[page_size - 1] == 'A' && bytes[page_size] == 'B', 17);
    void* base = const_cast<char*>(bytes);
    check(user::sys_free(static_cast<char*>(base) + page_size) == abi_call::invalid_argument, 18);
    check(user::sys_free(base) == 0 && user::sys_free(base) == abi_call::invalid_argument, 19);
    check(user::sys_write(fd, base, 1) == abi_call::bad_address, 20);
    auto* reused = static_cast<volatile char*>(user::sys_alloc(page_size + 1));
    check(reused == bytes && reused[page_size - 1] == 0 && reused[page_size] == 0, 21);
    constexpr char memory[] = "User memory allocated, released and reused\r\n";
    check(user::sys_write(fd, memory, sizeof(memory) - 1) == sizeof(memory) - 1, 22);
    user::sys_exit(42);
}

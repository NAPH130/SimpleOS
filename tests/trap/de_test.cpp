#include "test_boot.hpp"

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();
    kprintf_log(LogLevel::DEBUG, "before integer divide by zero\n");
    asm volatile("xor %%edx, %%edx; divq %%rdx" ::: "rax", "rdx", "cc", "memory");
    unexpected_trap_return();
}

#include "test_boot.hpp"

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();
    kprintf_log(LogLevel::DEBUG, "before integer divide by zero\n");
    // 由 CPU 执行零除数的 DIV 来检测异常。clobber 列出指令涉及的隐式寄存器。
    asm volatile("xor %%edx, %%edx; divq %%rdx" ::: "rax", "rdx", "cc", "memory");
    unexpected_trap_return();
}

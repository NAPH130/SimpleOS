#include "test_boot.hpp"

// 单独导出访存指令的入口，脚本用它的地址核对 #PF 日志中的 RIP。
extern "C" void trap_test_read_address(uintptr_t address);
asm(
    ".pushsection .text\n"
    ".global trap_test_read_address\n"
    ".type trap_test_read_address, @function\n"
    "trap_test_read_address:\n"
    "    movq (%rdi), %rax\n"
    "    ret\n"
    ".size trap_test_read_address, .-trap_test_read_address\n"
    ".popsection\n"
);

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();
    constexpr uintptr_t unmapped_address = 0x1000;
    kprintf_log(LogLevel::DEBUG, "before unmapped read at %p, address=%p\n",
                reinterpret_cast<void*>(&trap_test_read_address),
                reinterpret_cast<void*>(unmapped_address));
    trap_test_read_address(unmapped_address);
    unexpected_trap_return();
}

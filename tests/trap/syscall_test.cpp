#include "test_boot.hpp"
#include "user_pages.hpp"
#include <arch/x86_64/context.hpp>

extern "C" const uint8_t syscall_user_begin[], syscall_user_end[];

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();
    if (!user_pages::prepare(syscall_user_begin, syscall_user_end)) {
        kprintf_log(LogLevel::ERROR, "Cannot prepare syscall test pages.\n");
        x86_64::halt();
    }

    // 模拟 AMD64 函数入口的栈对齐。入口函数最终调用 sys_exit。
    constexpr uintptr_t entry_stack = user_pages::stack_top - sizeof(uint64_t);
    kprintf_log(LogLevel::DEBUG, "before Ring 3 syscall test\n");
    const int status = x86_64::run_user_mode(user_pages::code_address, entry_stack);
    kprintf_log(LogLevel::INFO, "kernel resumed after sys_exit, status=%d\n", status);
    x86_64::halt();
}

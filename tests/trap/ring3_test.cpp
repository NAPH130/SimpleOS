#include "test_boot.hpp"
#include "user_pages.hpp"
#include <arch/x86_64/context.hpp>
#include <stddef.h>
#include <stdint.h>

extern "C" const uint8_t ring3_program_start[], ring3_program_fault[], ring3_program_end[];
asm(
    ".pushsection .rodata\n"
    ".global ring3_program_start, ring3_program_fault, ring3_program_end\n"
    "ring3_program_start:\n"
    "    pushq $0x2a\n"
    "ring3_program_fault:\n"
    "    ud2\n"
    "ring3_program_end:\n"
    ".popsection\n"
);

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();

    // 测试指令进入 Ring 3 后通过 pushq 写入用户栈，再用 ud2 触发 #UD。
    if (!user_pages::prepare(ring3_program_start, ring3_program_end)) {
        kprintf_log(LogLevel::ERROR, "Cannot prepare Ring 3 test pages.\n");
        x86_64::halt();
    }

    const auto program_start = reinterpret_cast<uintptr_t>(ring3_program_start);
    const auto fault_offset = reinterpret_cast<uintptr_t>(ring3_program_fault) - program_start;
    kprintf_log(LogLevel::DEBUG, "before entering Ring 3, expected RIP=%p, RSP=%p\n",
                reinterpret_cast<void*>(user_pages::code_address + fault_offset),
                reinterpret_cast<void*>(user_pages::stack_top - sizeof(uint64_t)));
    x86_64::enter_user_mode(user_pages::code_address, user_pages::stack_top);
}

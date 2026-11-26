#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/trap.hpp>
#include <boot/limine.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>
#include <stdint.h>

// mov 写 SS 的指令位于函数入口，以便将实际故障 RIP 与预期指令地址比较。
extern "C" void trap_test_load_stack_segment(uint16_t selector);
asm(
    ".pushsection .text\n"
    ".global trap_test_load_stack_segment\n"
    ".type trap_test_load_stack_segment, @function\n"
    "trap_test_load_stack_segment:\n"
    "    movw %di, %ss\n"
    "    ret\n"
    ".size trap_test_load_stack_segment, .-trap_test_load_stack_segment\n"
    ".popsection\n"
);

extern "C" [[noreturn]] void kernel_main() {
    serial::init();
    if (!boot::limine_base_revision_supported()) {
        kprintf_log(LogLevel::ERROR, "Unsupported Limine base revision.\n");
        x86_64::halt();
    }

    x86_64::initialize_traps();
    uint16_t code_selector;
    asm volatile("movw %%cs, %0" : "=r"(code_selector));

    // SS 要求可写数据段，将当前代码段选择子写入 SS 会触发 #GP。
    // 错误码保留选择子的索引和表选择位，低两位用于 IDT 和 EXT 标志。
    const unsigned expected_error = static_cast<unsigned>(code_selector) & ~3u;
    kprintf_log(LogLevel::DEBUG, "before mov ss at %p, expected error=0x%x\n",
                reinterpret_cast<void*>(&trap_test_load_stack_segment), expected_error);
    trap_test_load_stack_segment(code_selector);
    kprintf_log(LogLevel::ERROR, "Unexpected return from mov ss.\n");
    x86_64::halt();
}

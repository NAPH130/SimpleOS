#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/trap.hpp>
#include <boot/limine.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>

// 用固定的非法指令触发 #UD，脚本可以根据函数入口核对故障位置。
extern "C" void trap_test_ud2();
asm(
    ".pushsection .text\n"
    ".global trap_test_ud2\n"
    ".type trap_test_ud2, @function\n"
    "trap_test_ud2:\n"
    "    ud2\n"
    "    ret\n"
    ".size trap_test_ud2, .-trap_test_ud2\n"
    ".popsection\n"
);

extern "C" [[noreturn]] void kernel_main() {
    serial::init();
    if (!boot::limine_base_revision_supported()) {
        kprintf_log(LogLevel::ERROR, "Unsupported Limine base revision.\n");
        x86_64::halt();
    }

    x86_64::initialize_traps();
    kprintf_log(LogLevel::DEBUG, "before ud2 at %p\n", reinterpret_cast<void*>(&trap_test_ud2));
    trap_test_ud2();
    kprintf_log(LogLevel::ERROR, "Unexpected return from ud2.\n");
    x86_64::halt();
}

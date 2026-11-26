#pragma once

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/trap.hpp>
#include <boot/limine.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>

// 每个自检镜像都有自己的 kernel_main。失败时留下串口诊断并停机。
inline void require_trap_test(bool passed, const char* diagnostic) {
    if (!passed) {
        kprintf_log(LogLevel::ERROR, "%s\n", diagnostic);
        x86_64::halt();
    }
}

inline void initialize_trap_test() {
    serial::init();
    require_trap_test(boot::limine_base_revision_supported(), "Unsupported Limine base revision.");
    x86_64::initialize_traps();
}

[[noreturn]] inline void unexpected_trap_return() {
    kprintf_log(LogLevel::ERROR, "Unexpected return from exception test.\n");
    x86_64::halt();
}

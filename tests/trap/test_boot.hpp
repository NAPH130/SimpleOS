#pragma once

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/trap.hpp>
#include <boot/limine.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>

inline void initialize_trap_test() {
    serial::init();
    if (!boot::limine_base_revision_supported()) {
        kprintf_log(LogLevel::ERROR, "Unsupported Limine base revision.\n");
        x86_64::halt();
    }
    x86_64::initialize_traps();
}

[[noreturn]] inline void unexpected_trap_return() {
    kprintf_log(LogLevel::ERROR, "Unexpected return from exception test.\n");
    x86_64::halt();
}

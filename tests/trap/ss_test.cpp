#include "test_boot.hpp"
#include <stddef.h>

namespace {

struct [[gnu::packed]] GdtRegister {
    uint16_t limit;
    uintptr_t base;
};

uint64_t test_gdt[32];

uint16_t append_absent_stack_descriptor() {
    GdtRegister original;
    asm volatile("sgdt %0" : "=m"(original));
    const size_t size = static_cast<size_t>(original.limit) + 1;
    const size_t count = size / sizeof(uint64_t);
    if (size % sizeof(uint64_t) != 0 || count >= sizeof(test_gdt) / sizeof(test_gdt[0])) {
        kprintf_log(LogLevel::ERROR, "Unexpected GDT layout in #SS test.\n");
        x86_64::halt();
    }

    const auto* entries = reinterpret_cast<const uint64_t*>(original.base);
    for (size_t i = 0; i < count; ++i) {
        test_gdt[i] = entries[i];
    }
    // Ring 0 可写数据段，P=0。加载到 SS 时，CPU 报告 #SS(selector)。
    test_gdt[count] = 0x00cf13000000ffff;
    const GdtRegister descriptor{static_cast<uint16_t>(size + sizeof(uint64_t) - 1),
                                 reinterpret_cast<uintptr_t>(test_gdt)};
    asm volatile("lgdt %0" : : "m"(descriptor) : "memory");
    return static_cast<uint16_t>(size);
}

}

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();
    const uint16_t selector = append_absent_stack_descriptor();
    kprintf_log(LogLevel::DEBUG, "before absent stack segment, expected error=0x%x\n",
                static_cast<unsigned>(selector));
    asm volatile("movw %0, %%ss" : : "r"(selector) : "memory");
    unexpected_trap_return();
}

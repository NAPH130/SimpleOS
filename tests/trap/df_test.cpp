#include "test_boot.hpp"
#include <arch/x86_64/vectors.h>
#include <stddef.h>

namespace {

struct [[gnu::packed]] IdtRegister {
    uint16_t limit;
    uintptr_t base;
};

void disable_general_protection_gate() {
    IdtRegister idtr;
    asm volatile("sidt %0" : "=m"(idtr));

    // 测试专用。让 CPU 递送 #GP 时遇到门不存在的 #NP，从而生成 #DF。
    constexpr size_t gate_size = 16;
    constexpr size_t attributes_offset = 5;
    auto* attributes = reinterpret_cast<volatile uint8_t*>(
        idtr.base + X86_64_VECTOR_GENERAL_PROTECTION * gate_size + attributes_offset);
    *attributes &= 0x7f;
}

}

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();
    disable_general_protection_gate();
    kprintf_log(LogLevel::DEBUG, "before general protection fault with absent gate\n");
    // 执行和 #GP 测试相同的非法段加载，此时递送 #GP 会引发第二个异常。
    asm volatile("movw %%cs, %%ax; movw %%ax, %%ss" ::: "rax", "memory");
    unexpected_trap_return();
}

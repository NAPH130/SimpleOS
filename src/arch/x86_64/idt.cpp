#include <arch/x86_64/gdt.hpp>
#include <arch/x86_64/segments.h>
#include <arch/x86_64/trap.hpp>
#include <arch/x86_64/vectors.h>

#define CPU_EXCEPTION(vector, error, name) extern "C" void x86_64_exception_ ## vector();
#include <arch/x86_64/exception_vectors.inc>
#undef CPU_EXCEPTION

namespace {

struct [[gnu::packed]] IdtGate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
};

struct [[gnu::packed]] IdtRegister {
    uint16_t limit;
    uint64_t base;
};

static_assert(sizeof(IdtGate) == 16);
static_assert(sizeof(IdtRegister) == 10);

alignas(16) IdtGate idt[256]{};
constexpr uint8_t kernel_interrupt_gate = 0x8e; // P=1，DPL=0，64 位中断门。

void set_exception_gate(uint8_t vector, void (*handler)(), uint8_t ist) {
    const auto entry = reinterpret_cast<uintptr_t>(handler);
    idt[vector] = {
        static_cast<uint16_t>(entry),
        X86_64_KERNEL_CODE_SELECTOR,
        ist,
        kernel_interrupt_gate,
        static_cast<uint16_t>(entry >> 16),
        static_cast<uint32_t>(entry >> 32),
        0,
    };
}

}

void x86_64::initialize_traps() {
    asm volatile("cli" ::: "memory");

    initialize_segments();
#define CPU_EXCEPTION(vector, error, name) \
    set_exception_gate(vector, x86_64_exception_ ## vector, \
                       vector == X86_64_VECTOR_DOUBLE_FAULT ? X86_64_DOUBLE_FAULT_IST : 0);
#include <arch/x86_64/exception_vectors.inc>
#undef CPU_EXCEPTION
    const IdtRegister idtr{sizeof(idt) - 1, reinterpret_cast<uintptr_t>(idt)};
    asm volatile("lidt %0" : : "m"(idtr) : "memory");
}

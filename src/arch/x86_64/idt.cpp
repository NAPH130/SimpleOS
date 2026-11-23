#include <arch/x86_64/trap.hpp>
#include <arch/x86_64/vectors.h>

extern "C" void x86_64_breakpoint_entry();

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

}

void x86_64::initialize_traps() {
    asm volatile("cli" ::: "memory");

    // 使用引导器已经加载的内核代码段选择子。
    uint16_t code_selector;
    asm volatile("movw %%cs, %0" : "=r"(code_selector));
    const auto entry = reinterpret_cast<uintptr_t>(&x86_64_breakpoint_entry);
    idt[X86_64_VECTOR_BREAKPOINT] = {
        static_cast<uint16_t>(entry),
        code_selector,
        0,
        kernel_interrupt_gate,
        static_cast<uint16_t>(entry >> 16),
        static_cast<uint32_t>(entry >> 32),
        0,
    };

    // TODO: 注册其余 CPU 异常及设备中断。用户态入口需要 GDT 和 TSS。
    const IdtRegister idtr{sizeof(idt) - 1, reinterpret_cast<uintptr_t>(idt)};
    asm volatile("lidt %0" : : "m"(idtr) : "memory");
}

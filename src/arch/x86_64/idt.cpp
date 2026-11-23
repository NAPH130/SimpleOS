#include <arch/x86_64/gdt.hpp>
#include <arch/x86_64/segments.h>
#include <arch/x86_64/trap.hpp>
#include <arch/x86_64/vectors.h>

// 用同一份向量表声明 trap_entry.S 生成的入口符号，例如 x86_64_exception_3。
#define CPU_EXCEPTION(vector, error, name) extern "C" void x86_64_exception_ ## vector();
#include <arch/x86_64/exception_vectors.inc>
#undef CPU_EXCEPTION

namespace {

// IDT 每项存一个入口及其属性。字段偏移由 CPU 规定，packed 消除编译器填充。
struct [[gnu::packed]] IdtGate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist; // 0 使用通常的栈选择规则，非零选择 TSS 中的 IST 栈。
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
};

struct [[gnu::packed]] IdtRegister {
    uint16_t limit; // 表的最后一个有效字节偏移，等于表大小减一。
    uint64_t base;
};

static_assert(sizeof(IdtGate) == 16);
static_assert(sizeof(IdtRegister) == 10);

alignas(16) IdtGate idt[256]{};
constexpr uint8_t kernel_interrupt_gate = 0x8e; // P=1，DPL=0，64 位中断门。

void set_exception_gate(uint8_t vector, void (*handler)(), uint8_t ist) {
    const auto entry = reinterpret_cast<uintptr_t>(handler);
    // CPU 触发该向量时，从这三段 offset 字段拼出 64 位入口地址。
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
    // 安装描述符期间屏蔽外部可屏蔽中断。CPU 执行指令时仍会报告同步异常。
    asm volatile("cli" ::: "memory");

    // 先准备 IDT 门引用的内核代码段，以及 #DF 使用的 TSS 应急栈。
    initialize_segments();
#define CPU_EXCEPTION(vector, error, name) \
    set_exception_gate(vector, x86_64_exception_ ## vector, \
                       vector == X86_64_VECTOR_DOUBLE_FAULT ? X86_64_DOUBLE_FAULT_IST : 0);
#include <arch/x86_64/exception_vectors.inc>
#undef CPU_EXCEPTION
    // lidt 把表的起始地址和范围载入 IDTR，后续 CPU 通过向量号索引这张表。
    const IdtRegister idtr{sizeof(idt) - 1, reinterpret_cast<uintptr_t>(idt)};
    asm volatile("lidt %0" : : "m"(idtr) : "memory");
}

#include <arch/x86_64/gdt.hpp>
#include <arch/x86_64/segments.h>
#include <stddef.h>
#include <stdint.h>

extern "C" void x86_64_load_gdt(const void* descriptor);

namespace {

struct [[gnu::packed]] TableRegister {
    uint16_t limit;
    uint64_t base;
};

// 64 位任务状态段。RSP0 用于进入 Ring 0，IST 为特定异常提供独立栈。
struct [[gnu::packed]] Tss {
    uint32_t reserved;
    uint64_t rsp[3];
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t io_map_base;
};

static_assert(sizeof(TableRegister) == 10);
static_assert(sizeof(Tss) == 104);
static_assert(offsetof(Tss, rsp) == 4);
static_assert(offsetof(Tss, ist) == 36);

alignas(16) Tss tss{};
alignas(16) uint8_t ring0_entry_stack[16384];
alignas(16) uint8_t double_fault_stack[8192];
alignas(16) uint64_t gdt[] = {
    0,
    0x00af9b000000ffff, // Ring 0, 64-bit code
    0x00cf93000000ffff, // Ring 0, writable data
    0, 0,              // 16-byte TSS descriptor
    0x00affb000000ffff, // Ring 3 代码段，DPL=3，L=1，D=0。
    0x00cff3000000ffff, // Ring 3 可写数据段，也可作为用户栈段。
};

constexpr size_t tss_slot = X86_64_TSS_SELECTOR >> 3;
constexpr size_t user_code_slot = X86_64_USER_CODE_SELECTOR >> 3;
constexpr size_t user_data_slot = X86_64_USER_DATA_SELECTOR >> 3;
static_assert(user_code_slot == tss_slot + 2 && user_data_slot == user_code_slot + 1);
static_assert(sizeof(gdt) / sizeof(gdt[0]) == user_data_slot + 1);

}

void x86_64::initialize_segments() {
    // 普通异常门将 Ring 3 切入 Ring 0 时，CPU 从 RSP0 取内核栈顶。
    // 栈向低地址增长，所以初始栈指针设在缓冲区末尾。
    tss.rsp[0] = reinterpret_cast<uintptr_t>(ring0_entry_stack + sizeof(ring0_entry_stack));

    // 硬件 IST 编号从 1 开始，数组从 0 开始，写入的是应急栈的顶部地址。
    tss.ist[X86_64_DOUBLE_FAULT_IST - 1] =
        reinterpret_cast<uintptr_t>(double_fault_stack + sizeof(double_fault_stack));
    tss.io_map_base = sizeof(tss); // 将 I/O 位图偏移设到 TSS 的有效范围之外。

    const uint64_t base = reinterpret_cast<uintptr_t>(&tss);
    // 64 位 TSS 描述符占两个 GDT 槽。第二个槽保存基址的高 32 位。
    gdt[tss_slot] = (sizeof(tss) - 1) | ((base & 0xffffff) << 16)
        | (0x89ull << 40) | ((base & 0xff000000) << 32);
    gdt[tss_slot + 1] = base >> 32;
    const TableRegister descriptor{sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(gdt)};
    x86_64_load_gdt(&descriptor);
}

bool x86_64::on_double_fault_stack(const void* address) {
    return reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(double_fault_stack)
        < sizeof(double_fault_stack);
}

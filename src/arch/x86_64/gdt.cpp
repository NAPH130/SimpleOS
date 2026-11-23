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
alignas(16) uint8_t double_fault_stack[8192];
alignas(16) uint64_t gdt[] = {
    0,
    0x00af9b000000ffff, // Ring 0, 64-bit code
    0x00cf93000000ffff, // Ring 0, writable data
    0, 0,              // 16-byte TSS descriptor
};

}

void x86_64::initialize_segments() {
    tss.ist[X86_64_DOUBLE_FAULT_IST - 1] =
        reinterpret_cast<uintptr_t>(double_fault_stack + sizeof(double_fault_stack));
    tss.io_map_base = sizeof(tss);

    const uint64_t base = reinterpret_cast<uintptr_t>(&tss);
    const size_t slot = X86_64_TSS_SELECTOR / 8;
    gdt[slot] = (sizeof(tss) - 1) | ((base & 0xffffff) << 16)
        | (0x89ull << 40) | ((base & 0xff000000) << 32);
    gdt[slot + 1] = base >> 32;
    const TableRegister descriptor{sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(gdt)};
    x86_64_load_gdt(&descriptor);
}

bool x86_64::on_double_fault_stack(const void* address) {
    return reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(double_fault_stack)
        < sizeof(double_fault_stack);
}

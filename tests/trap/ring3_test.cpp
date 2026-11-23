#include "test_boot.hpp"
#include <arch/x86_64/context.hpp>
#include <stddef.h>
#include <stdint.h>

extern "C" const uint8_t ring3_program_start[], ring3_program_fault[], ring3_program_end[];
asm(
    ".pushsection .rodata\n"
    ".global ring3_program_start, ring3_program_fault, ring3_program_end\n"
    "ring3_program_start:\n"
    "    pushq $0x2a\n"
    "ring3_program_fault:\n"
    "    ud2\n"
    "ring3_program_end:\n"
    ".popsection\n"
);

namespace {

struct HhdmResponse {
    uint64_t revision;
    uint64_t offset;
};

struct ExecutableAddressResponse {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};

template <typename Response>
struct LimineRequest {
    uint64_t id[4];
    uint64_t revision;
    const Response* response;
};

// 仅测试镜像请求这些地址，用于访问 CR3 的页表和定位测试数据的物理页。
constexpr uint64_t limine_magic_a = 0xc7b1dd30df4c8b88;
constexpr uint64_t limine_magic_b = 0x0a82e883a194f07b;
[[gnu::used, gnu::section(".limine_requests")]]
volatile LimineRequest<HhdmResponse> hhdm_request{
    {limine_magic_a, limine_magic_b, 0x48dcf1cb8ad2b852, 0x63984e959a98244b}, 0, nullptr
};
[[gnu::used, gnu::section(".limine_requests")]]
volatile LimineRequest<ExecutableAddressResponse> executable_address_request{
    {limine_magic_a, limine_magic_b, 0x71ba76863cc55f63, 0xb2644a48c516a487}, 0, nullptr
};

constexpr size_t page_size = 4096;
constexpr uint64_t page_present = 1ull << 0;
constexpr uint64_t page_writable = 1ull << 1;
constexpr uint64_t page_user = 1ull << 2;
constexpr uint64_t page_no_execute = 1ull << 63;
constexpr uint64_t page_address_mask = 0x000ffffffffff000;
constexpr uintptr_t user_code_address = 0x400000;
constexpr uintptr_t user_stack_address = user_code_address + page_size;
constexpr uintptr_t user_stack_top = user_stack_address + page_size;

struct alignas(page_size) PageTable {
    uint64_t entries[page_size / sizeof(uint64_t)];
};

// 页表、用户代码与用户栈各占各自的物理页，测试镜像预留这些空间。
PageTable pdpt{}, page_directory{}, page_table{};
alignas(page_size) uint8_t user_code[page_size]{};
alignas(page_size) uint8_t user_stack[page_size]{};

size_t page_index(uintptr_t address, unsigned shift) {
    return (address >> shift) & 0x1ff;
}

uintptr_t physical_address(const ExecutableAddressResponse& image, const void* address) {
    return image.physical_base + (reinterpret_cast<uintptr_t>(address) - image.virtual_base);
}

uint64_t stack_no_execute_flag() {
    // IA32_EFER.NXE 开启时，页表项的第 63 位可禁止执行栈上的指令。
    constexpr uint32_t efer_msr = 0xc0000080;
    constexpr uint32_t nxe = 1u << 11;
    uint32_t efer_low, efer_high;
    asm volatile("rdmsr" : "=a"(efer_low), "=d"(efer_high) : "c"(efer_msr));
    return (efer_low & nxe) != 0 ? page_no_execute : 0;
}

bool install_user_pages() {
    const auto* hhdm = hhdm_request.response;
    const auto* image = executable_address_request.response;
    uintptr_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    constexpr uintptr_t la57 = 1ull << 12;
    if (hhdm == nullptr || image == nullptr || (cr4 & la57) != 0) {
        return false;
    }

    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    auto* pml4 = reinterpret_cast<PageTable*>(hhdm->offset + (cr3 & page_address_mask));
    uint64_t& root_entry = pml4->entries[page_index(user_code_address, 39)];
    if ((root_entry & page_present) != 0) {
        return false;
    }

    // 从叶子往上连接。在空闲的低地址表项中发布分支，保留 Limine 建好的高地址映射。
    constexpr uint64_t user_read = page_present | page_user;
    constexpr uint64_t user_write = user_read | page_writable;
    page_table.entries[page_index(user_code_address, 12)] =
        physical_address(*image, user_code) | user_read;
    page_table.entries[page_index(user_stack_address, 12)] =
        physical_address(*image, user_stack) | user_write | stack_no_execute_flag();
    page_directory.entries[page_index(user_code_address, 21)] =
        physical_address(*image, &page_table) | user_write;
    pdpt.entries[page_index(user_code_address, 30)] =
        physical_address(*image, &page_directory) | user_write;
    root_entry = physical_address(*image, &pdpt) | user_write;
    asm volatile("invlpg (%0)" : : "r"(user_code_address) : "memory");
    asm volatile("invlpg (%0)" : : "r"(user_stack_address) : "memory");
    return true;
}

}

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();

    // 复制位置无关的测试指令。进入 Ring 3 后，pushq 写入用户栈，ud2 触发 #UD。
    const auto program_start = reinterpret_cast<uintptr_t>(ring3_program_start);
    const auto program_size = reinterpret_cast<uintptr_t>(ring3_program_end) - program_start;
    if (program_size > sizeof(user_code)) {
        kprintf_log(LogLevel::ERROR, "Ring 3 test program exceeds one page.\n");
        x86_64::halt();
    }
    for (size_t offset = 0; offset < program_size; ++offset) {
        user_code[offset] = ring3_program_start[offset];
    }

    if (!install_user_pages()) {
        kprintf_log(LogLevel::ERROR, "Cannot install Ring 3 test pages in the boot page table.\n");
        x86_64::halt();
    }

    const auto fault_offset = reinterpret_cast<uintptr_t>(ring3_program_fault) - program_start;
    kprintf_log(LogLevel::DEBUG, "before entering Ring 3, expected RIP=%p, RSP=%p\n",
                reinterpret_cast<void*>(user_code_address + fault_offset),
                reinterpret_cast<void*>(user_stack_top - sizeof(uint64_t)));
    x86_64::enter_user_mode(user_code_address, user_stack_top);
}

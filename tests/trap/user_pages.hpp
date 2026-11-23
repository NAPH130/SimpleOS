#pragma once

#include <arch/x86_64/paging.hpp>
#include <boot/limine.hpp>

namespace user_pages {

using namespace x86_64::paging;

inline constexpr uintptr_t code_address = 0x400000;
inline constexpr uintptr_t stack_address = code_address + page_size;
inline constexpr uintptr_t stack_top = stack_address + page_size;

namespace {

struct ExecutableAddressResponse {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};

struct ExecutableAddressRequest {
    uint64_t id[4];
    uint64_t revision;
    const ExecutableAddressResponse* response;
};

// 测试镜像用加载地址定位预留的物理页。
[[gnu::used, gnu::section(".limine_requests")]]
volatile ExecutableAddressRequest executable_address_request{
    {0xc7b1dd30df4c8b88, 0x0a82e883a194f07b, 0x71ba76863cc55f63, 0xb2644a48c516a487},
    0,
    nullptr,
};

struct alignas(page_size) PageTable {
    uint64_t entries[page_size / sizeof(uint64_t)];
};

// 页表、用户代码与用户栈各占各自的物理页，测试镜像预留这些空间。
PageTable pdpt{}, page_directory{}, page_table{};
alignas(page_size) uint8_t user_code[page_size]{};
alignas(page_size) uint8_t user_stack[page_size]{};

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
    const auto* image = executable_address_request.response;
    uintptr_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    constexpr uintptr_t la57 = 1ull << 12;
    uintptr_t direct_map;
    if (image == nullptr || (cr4 & la57) != 0 || !boot::higher_half_direct_map_offset(direct_map)) {
        return false;
    }

    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    auto* pml4 = reinterpret_cast<PageTable*>(direct_map + (cr3 & page_address_mask));
    uint64_t& root_entry = pml4->entries[page_index(code_address, 39)];
    if ((root_entry & page_present) != 0) {
        return false;
    }

    // 从叶子往上连接。在空闲的低地址表项中发布分支，保留 Limine 建好的高地址映射。
    constexpr uint64_t user_read = page_present | page_user;
    constexpr uint64_t user_write = user_read | page_writable;
    page_table.entries[page_index(code_address, 12)] =
        physical_address(*image, user_code) | user_read;
    page_table.entries[page_index(stack_address, 12)] =
        physical_address(*image, user_stack) | user_write | stack_no_execute_flag();
    page_directory.entries[page_index(code_address, 21)] =
        physical_address(*image, &page_table) | user_write;
    pdpt.entries[page_index(code_address, 30)] =
        physical_address(*image, &page_directory) | user_write;
    root_entry = physical_address(*image, &pdpt) | user_write;
    asm volatile("invlpg (%0)" : : "r"(code_address) : "memory");
    asm volatile("invlpg (%0)" : : "r"(stack_address) : "memory");
    return true;
}

}

// 此夹具在独立镜像中使用空闲的低地址页表分支，只准备一次。
bool prepare(const uint8_t* program_begin, const uint8_t* program_end) {
    const size_t size = reinterpret_cast<uintptr_t>(program_end)
        - reinterpret_cast<uintptr_t>(program_begin);
    if (size == 0 || size > sizeof(user_code) || !install_user_pages()) {
        return false;
    }
    for (size_t offset = 0; offset < size; ++offset) {
        user_code[offset] = program_begin[offset];
    }
    return true;
}

}

#include <arch/x86_64/paging.hpp>
#include <boot/limine.hpp>

namespace x86_64::paging {
namespace {

bool user_page_present(uintptr_t address, uintptr_t root, uintptr_t direct_map,
                       unsigned root_shift) {
    constexpr uint64_t required_flags = page_present | page_user;
    constexpr uint64_t large_page = 1ull << 7;
    uintptr_t table = root;
    for (unsigned shift = root_shift; ; shift -= 9) {
        const auto* entries = reinterpret_cast<const uint64_t*>(direct_map + table);
        const uint64_t entry = entries[page_index(address, shift)];
        // 每一级都须允许用户访问。2 MiB 和 1 GiB 大页提前结束遍历。
        if ((entry & required_flags) != required_flags) {
            return false;
        }
        if (shift == 12 || ((shift == 21 || shift == 30) && (entry & large_page) != 0)) {
            return true;
        }
        table = entry & page_address_mask;
    }
}

}

bool user_buffer_readable(uintptr_t address, size_t size) {
    if (size == 0) {
        return true;
    }

    uintptr_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    constexpr uintptr_t la57 = 1ull << 12;
    constexpr uintptr_t smap = 1ull << 21;
    constexpr uintptr_t pke = 1ull << 22;
    const unsigned root_shift = (cr4 & la57) != 0 ? 48 : 39;
    const uintptr_t user_limit = uintptr_t{1} << (root_shift + 8);
    // SimpleOS 把用户空间置于低半区。用减法检查区间，避免地址加长度溢出。
    if (address == 0 || address >= user_limit || size > user_limit - address) {
        return false;
    }

    uintptr_t direct_map;
    // 当前内核直接读取用户指针，启用 SMAP 或保护键后需要专门的用户访问路径。
    if ((cr4 & (smap | pke)) != 0 || !boot::higher_half_direct_map_offset(direct_map)) {
        return false;
    }

    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");
    const uintptr_t last_page = (address + size - 1) & ~(page_size - 1);
    for (uintptr_t page = address & ~(page_size - 1); page <= last_page; page += page_size) {
        if (!user_page_present(page, cr3 & page_address_mask, direct_map, root_shift)) {
            return false;
        }
    }
    return true;
}

}

#include <arch/x86_64/paging.hpp>
#include <arch/x86_64/cpu.hpp>
#include <boot/limine.hpp>
#include <kernel/console.hpp>
#include <mm/memory.hpp>

namespace x86_64::paging {
namespace {

constexpr unsigned page_shift = 12;
constexpr unsigned index_bits = 9;
constexpr size_t entries_per_table = page_size / sizeof(uint64_t);
constexpr size_t max_levels = 5;
constexpr uint64_t user_page_flags = page_present | page_user;
constexpr uint64_t large_page = 1ull << 7;
// 位 9 由软件使用。在本模块创建的表项中，它表示对应帧由页帧分配器提供。
constexpr uint64_t owned_entry = 1ull << 9;
constexpr uint64_t owned_table_flags = user_page_flags | page_writable | owned_entry;

struct CurrentPageTable {
    uintptr_t root;       // CR3 中顶层页表的物理地址。
    uintptr_t direct_map; // 物理页加此偏移后可由内核读写。
    unsigned root_shift;  // 四级页表为 39，五级页表为 48。

    uintptr_t user_limit() const {
        return uintptr_t{1} << (root_shift + index_bits - 1);
    }

    uint64_t* entries(uintptr_t physical_address) const {
        return reinterpret_cast<uint64_t*>(direct_map + physical_address);
    }
};

// 保存当前访问路径，以便撤销叶子表项后从底向顶回收空表。
struct PagePath {
    uint64_t* slots[max_levels];
    size_t depth = 0;
    unsigned shift = 0;

    uint64_t& last() const {
        return *slots[depth - 1];
    }
};

uintptr_t read_cr4() {
    uintptr_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

bool current_page_table(uintptr_t cr4, CurrentPageTable& table) {
    if (!boot::higher_half_direct_map_offset(table.direct_map)) {
        return false;
    }
    constexpr uintptr_t la57 = 1ull << 12;
    table.root_shift = (cr4 & la57) != 0 ? 48 : 39;

    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");
    table.root = cr3 & page_address_mask;
    return true;
}

bool is_large_page(uint64_t entry, unsigned shift) {
    return (shift == 21 || shift == 30) && (entry & large_page) != 0;
}

bool user_page_present(const CurrentPageTable& table, uintptr_t address) {
    uintptr_t physical_table = table.root;
    for (unsigned shift = table.root_shift; ; shift -= index_bits) {
        const uint64_t entry = table.entries(physical_table)[page_index(address, shift)];
        // 每一级都须允许用户访问。2 MiB 和 1 GiB 大页提前结束遍历。
        if ((entry & user_page_flags) != user_page_flags) {
            return false;
        }
        if (shift == page_shift || is_large_page(entry, shift)) {
            return true;
        }
        physical_table = entry & page_address_mask;
    }
}

bool find_user_path(const CurrentPageTable& table, uintptr_t address, PagePath& path) {
    if (address == 0 || address >= table.user_limit() || (address & (page_size - 1)) != 0) {
        return false;
    }

    uintptr_t physical_table = table.root;
    for (unsigned shift = table.root_shift; ; shift -= index_bits) {
        uint64_t& entry = table.entries(physical_table)[page_index(address, shift)];
        path.slots[path.depth++] = &entry;
        path.shift = shift;
        if (entry == 0 || shift == page_shift) {
            return true;
        }
        // 只沿自有的用户分支继续。引导器映射、保留的非零项和大页都不会被覆盖。
        if ((entry & owned_table_flags) != owned_table_flags || is_large_page(entry, shift)) {
            return false;
        }
        physical_table = entry & page_address_mask;
    }
}

uint64_t data_page_flags() {
    // IA32_EFER.NXE 启用后，表项的第 63 位才能用作禁止执行。
    constexpr uint32_t efer_msr = 0xc0000080;
    constexpr uint32_t nxe = 1u << 11;
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(efer_msr));
    return page_writable | ((low & nxe) != 0 ? page_no_execute : 0);
}

bool table_empty(const uint64_t* entries) {
    for (size_t index = 0; index < entries_per_table; ++index) {
        if (entries[index] != 0) {
            return false;
        }
    }
    return true;
}

void invalidate_page(uintptr_t address) {
    // 内存约束让清除或写入表项发生在失效指令之前。
    asm volatile("invlpg (%0)" : : "r"(address) : "memory");
}

void release_frames(const uintptr_t* frames, size_t count) {
    while (count != 0) {
        if (!mm::free_frame(frames[--count])) {
            // 这里的帧已确认属于本模块。释放失败表示内部所有权记录不一致。
            kprintf_log(LogLevel::ERROR, "Cannot release a frame owned by user paging.\n");
            halt();
        }
    }
}

}

bool user_buffer_readable(uintptr_t address, size_t size) {
    if (size == 0) {
        return true;
    }

    const uintptr_t cr4 = read_cr4();
    constexpr uintptr_t smap = 1ull << 21;
    constexpr uintptr_t pke = 1ull << 22;
    CurrentPageTable table;
    // 当前内核直接读取用户指针，启用 SMAP 或保护键后需要专门的用户访问路径。
    if ((cr4 & (smap | pke)) != 0 || !current_page_table(cr4, table)) {
        return false;
    }
    const uintptr_t user_limit = table.user_limit();
    // SimpleOS 把用户空间置于低半区。用减法检查区间，避免地址加长度溢出。
    if (address == 0 || address >= user_limit || size > user_limit - address) {
        return false;
    }

    const uintptr_t last_page = (address + size - 1) & ~(page_size - 1);
    for (uintptr_t page = address & ~(page_size - 1); page <= last_page; page += page_size) {
        if (!user_page_present(table, page)) {
            return false;
        }
    }
    return true;
}

bool map_user_page(uintptr_t virtual_address, UserPageAccess access, uintptr_t& physical_address) {
    CurrentPageTable table;
    PagePath path;
    if (!current_page_table(read_cr4(), table) || !find_user_path(table, virtual_address, path)
        || path.last() != 0) {
        return false;
    }

    // 从第一个空表项往下需要一张数据页，以及每个缺失层级的一张页表页。
    // 在发布新分支前先申请全部帧。失败时可以直接归还，现有页表还没有被修改。
    uintptr_t frames[max_levels];
    const size_t needed = 1 + (path.shift - page_shift) / index_bits;
    for (size_t index = 0; index < needed; ++index) {
        if (!mm::allocate_frame(frames[index])) {
            release_frames(frames, index);
            return false;
        }
        auto* contents = table.entries(frames[index]);
        for (size_t word = 0; word < entries_per_table; ++word) {
            contents[word] = 0;
        }
    }

    uint64_t entry = frames[0] | user_page_flags | owned_entry;
    if (access == UserPageAccess::read_write) {
        entry |= data_page_flags();
    }
    // 从叶子向上连接新页表。最终一次写入 path.last()，让整个分支对 CPU 可见。
    for (size_t index = 1; index < needed; ++index) {
        const unsigned shift = page_shift + (index - 1) * index_bits;
        table.entries(frames[index])[page_index(virtual_address, shift)] = entry;
        entry = frames[index] | owned_table_flags;
    }
    path.last() = entry;
    invalidate_page(virtual_address);
    physical_address = frames[0];
    return true;
}

bool unmap_user_page(uintptr_t virtual_address) {
    CurrentPageTable table;
    PagePath path;
    constexpr uint64_t owned_leaf = user_page_flags | owned_entry;
    if (!current_page_table(read_cr4(), table) || !find_user_path(table, virtual_address, path)
        || path.shift != page_shift || (path.last() & owned_leaf) != owned_leaf) {
        return false;
    }

    uintptr_t frames[max_levels];
    size_t count = 0;
    frames[count++] = path.last() & page_address_mask;
    path.last() = 0;
    for (size_t level = path.depth - 1; level != 0; --level) {
        uint64_t& parent = *path.slots[level - 1];
        const uintptr_t child_frame = parent & page_address_mask;
        if (!table_empty(table.entries(child_frame))) {
            break;
        }
        parent = 0;
        frames[count++] = child_frame;
    }

    // 先撤掉叶子和空分支，再让旧转换失效。此后才能把物理帧交给其他调用者。
    invalidate_page(virtual_address);
    release_frames(frames, count);
    return true;
}

}

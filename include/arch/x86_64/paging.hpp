#pragma once

#include <stddef.h>
#include <stdint.h>

namespace x86_64::paging {

// x86-64 的普通页为 4 KiB，页表项的地址位与权限位共用一个 64 位整数。
inline constexpr size_t page_size = 4096;
inline constexpr uint64_t page_present = 1ull << 0;
inline constexpr uint64_t page_writable = 1ull << 1;
inline constexpr uint64_t page_user = 1ull << 2;
inline constexpr uint64_t page_no_execute = 1ull << 63;
inline constexpr uint64_t page_address_mask = 0x000ffffffffff000;

// 每级索引占 9 位。shift 给出当前级别最低的索引位，12 对应最后一级。
constexpr size_t page_index(uintptr_t address, unsigned shift) {
    return (address >> shift) & 0x1ff;
}

// 检查当前页表中的整个用户范围。调用方须保证校验和读取期间映射保持稳定。
[[nodiscard]] bool user_buffer_readable(uintptr_t address, size_t size);

enum class UserPageAccess {
    read_execute, // 用户代码页，写权限关闭。
    read_write,   // 用户数据或栈页，启用 NX 时执行权限关闭。
};

// 在当前 CR3 的用户范围内建立单个 4 KiB 页的映射，virtual_address 须按页对齐。
// 映射层会清零新页帧。成功时写出物理地址，失败时保留页表及输出参数。
// 调用方须串行修改当前页表，并保证它只在当前 CPU 上使用。
[[nodiscard]] bool map_user_page(uintptr_t virtual_address, UserPageAccess access,
                                 uintptr_t& physical_address);

// 删除 map_user_page 建立的映射，归还数据页和空的自有页表页。
// 调用方须先结束用户访问，并遵循同样的串行与单 CPU 约定。
[[nodiscard]] bool unmap_user_page(uintptr_t virtual_address);

}

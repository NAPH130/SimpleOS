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

// 当前 CR3 所引用的物理页表根。
uintptr_t current_page_table_root();
// 启动用户进程前保存完整 CR3，退出后恢复。新页表用自己的物理根作为 CR3。
uintptr_t read_cr3();
void write_cr3(uintptr_t value);

// 分配一个顶层页表，低半区初始为空，高半区共享当前的内核映射。
[[nodiscard]] bool create_user_page_table(uintptr_t& root);
// 归还非活动且低半区已清空的页表根。高半区页表由内核统一持有。
[[nodiscard]] bool destroy_user_page_table(uintptr_t root);

// 在页对齐的半开区间 [begin, end) 内查找首个可映射的连续范围。
// size 以字节计且按页对齐。成功时写出起点，调用方须串行完成后续建表。
[[nodiscard]] bool find_user_range(uintptr_t begin, uintptr_t end, size_t size,
                                  uintptr_t& virtual_address);
[[nodiscard]] bool find_user_range(uintptr_t root, uintptr_t begin, uintptr_t end, size_t size,
                                   uintptr_t& virtual_address);

// 在当前 CR3 或显式指定的 root 中建立单个 4 KiB 用户页映射，virtual_address 须按页对齐。
// 映射层会清零新页帧。成功时写出物理地址，失败时保留页表及输出参数。
// 调用方须串行修改当前页表，并保证它只在当前 CPU 上使用。
[[nodiscard]] bool map_user_page(uintptr_t virtual_address, UserPageAccess access,
                                 uintptr_t& physical_address);
[[nodiscard]] bool map_user_page(uintptr_t root, uintptr_t virtual_address, UserPageAccess access,
                                 uintptr_t& physical_address);

// 删除 map_user_page 建立的映射，归还数据页和空的自有页表页。
// 调用方须先结束用户访问，并遵循同样的串行与单 CPU 约定。
[[nodiscard]] bool unmap_user_page(uintptr_t virtual_address);
[[nodiscard]] bool unmap_user_page(uintptr_t root, uintptr_t virtual_address);

// 取得本模块建立的单页映射所对应的物理页帧，供内核通过直接映射装入数据。
[[nodiscard]] bool user_page_frame(uintptr_t virtual_address, uintptr_t& physical_address);
[[nodiscard]] bool user_page_frame(uintptr_t root, uintptr_t virtual_address, uintptr_t& physical_address);

}

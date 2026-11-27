#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mm {

struct PhysicalRange {
    uintptr_t base; // 区间起始物理地址。
    size_t size;    // 区间字节数，0 表示跳过此条目。
};

using PhysicalRangeReader = PhysicalRange (*)(size_t index);

// 初始化时按索引读取稳定的内存地图。可用区间按物理地址排序且互不重叠，
// 空区间会被跳过。direct_map_offset 提供对可用物理页的内核映射。
[[nodiscard]] bool initialize(size_t range_count, PhysicalRangeReader read_range,
                              uintptr_t direct_map_offset);
// 转换已经包含在高半区直接映射中的物理地址。初始化前或地址溢出时返回 nullptr。
[[nodiscard]] void* physical_to_virtual(uintptr_t physical_address);

// 内核堆在页帧分配器初始化后可用。分配按字节计，并满足 max_align_t 对齐。
// 0 字节或分配失败返回 nullptr。释放后，相邻空闲块合并，空内存段归还其页帧。
// 调用方负责串行访问。kfree 接收 kmalloc 返回的指针或 nullptr。
[[nodiscard]] void* kmalloc(size_t size);
void kfree(void* pointer);

// 分配一个 4 KiB 页帧并返回物理地址。失败时保留原输出值。调用方负责串行访问。
[[nodiscard]] bool allocate_frame(uintptr_t& physical_address);
// 成功时分配 count 张连续物理页帧。失败时位图和输出地址保持原值。
[[nodiscard]] bool allocate_contiguous_frames(size_t count, uintptr_t& physical_address);
// 仅释放由分配器占用的页帧。无效地址或重复释放返回 false。
[[nodiscard]] bool free_frame(uintptr_t physical_address);

// 连续虚拟区间的分配和释放见 mm/address_space.hpp。
class AddressSpace;

}

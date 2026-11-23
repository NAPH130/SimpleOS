#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mm {

struct PhysicalRange {
    uintptr_t base;
    size_t size;
};

using PhysicalRangeReader = PhysicalRange (*)(size_t index);

// 初始化时按索引读取稳定的内存地图。可用区间按物理地址排序且互不重叠，
// 空区间会被跳过。direct_map_offset 提供对可用物理页的内核映射。
[[nodiscard]] bool initialize(size_t range_count, PhysicalRangeReader read_range,
                              uintptr_t direct_map_offset);

// TODO: 实现可变分区分配及相邻空闲分区合并，分配失败时返回 nullptr。
[[nodiscard]] void* kmalloc(size_t size);
void kfree(void* pointer);

// 分配一个 4 KiB 页帧并返回物理地址。失败时保留原输出值。调用方负责串行访问。
[[nodiscard]] bool allocate_frame(uintptr_t& physical_address);
// 仅释放由分配器占用的页帧。无效地址或重复释放返回 false。
[[nodiscard]] bool free_frame(uintptr_t physical_address);

// TODO: 实现进程地址空间和用户页映射、权限及回收。
class AddressSpace;

}

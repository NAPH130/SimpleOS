#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mm {

struct PhysicalRange {
    uintptr_t base;
    size_t size;
};

// TODO: 从引导器内存映射提取可用区间，并初始化物理分配器和内核堆。
void initialize(const PhysicalRange* usable_ranges, size_t count);

// TODO: 实现可变分区分配及相邻空闲分区合并，分配失败时返回 nullptr。
[[nodiscard]] void* kmalloc(size_t size);
void kfree(void* pointer);

// TODO: 分配及释放物理页帧，为后续页表管理提供内存。
[[nodiscard]] bool allocate_frame(uintptr_t& physical_address);
void free_frame(uintptr_t physical_address);

// TODO: 实现进程地址空间和用户页映射、权限及回收。
class AddressSpace;

}

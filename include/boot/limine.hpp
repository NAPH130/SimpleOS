#pragma once

#include <stdint.h>

namespace boot {

// 确认引导器已接受内核声明的 Limine 基础协议版本。
bool limine_base_revision_supported();
// 物理地址加上 offset，可访问引导器直接映射的物理内存。
[[nodiscard]] bool higher_half_direct_map_offset(uintptr_t& offset);
// 将 Limine 的可用区间交给物理页帧分配器。
[[nodiscard]] bool initialize_physical_memory();

}

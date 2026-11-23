#pragma once

#include <stdint.h>

namespace boot {

bool limine_base_revision_supported();
// 物理地址加上 offset，可访问引导器直接映射的物理内存。
[[nodiscard]] bool higher_half_direct_map_offset(uintptr_t& offset);

}

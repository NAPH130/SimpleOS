#pragma once

#include <stddef.h>

namespace x86_64 {

void initialize_segments();
bool on_double_fault_stack(const void* address);
// 检查从 address 开始的整段现场是否位于 RSP0 对应的入口栈内。
bool on_ring0_entry_stack(const void* address, size_t size);

}

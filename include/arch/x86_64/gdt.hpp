#pragma once

#include <stddef.h>

namespace x86_64 {

// 安装并启用内核段、用户段和 TSS。初始化 IDT 前必须先准备其引用的代码段。
void initialize_segments();
// 检查现场起点是否位于 #DF 专用的 IST 栈内。
bool on_double_fault_stack(const void* address);
// 检查从 address 开始的整段现场是否位于 RSP0 对应的入口栈内。
bool on_ring0_entry_stack(const void* address, size_t size);

}

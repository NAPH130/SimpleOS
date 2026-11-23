#pragma once

#include <stdint.h>

namespace x86_64 {

// TODO: 与切换汇编一起定义内核任务的寄存器现场布局。
struct TaskContext;

// TODO: 按 AMD64 调用约定保存当前任务现场并恢复下一任务现场。
extern "C" void switch_to(TaskContext* current, const TaskContext* next);

// 调用前加载 GDT、TSS 和 IDT，并在当前页表中映射用户入口和可写的用户栈。
// 通过 iretq 从 entry 开始执行，使用 user_stack_top 作为 RSP，保持可屏蔽中断关闭。
extern "C" [[noreturn]] void enter_user_mode(uintptr_t entry, uintptr_t user_stack_top);

}

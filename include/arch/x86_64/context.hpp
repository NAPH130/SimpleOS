#pragma once

#include <stdint.h>

namespace x86_64 {

// TODO: 与切换汇编一起定义内核任务的寄存器现场布局。
struct TaskContext;

// TODO: 按 AMD64 调用约定保存当前任务现场并恢复下一任务现场。
extern "C" void switch_to(TaskContext* current, const TaskContext* next);

// TODO: 构造中断返回现场，通过 iretq 进入 Ring 3。
[[noreturn]] void enter_user_mode(uintptr_t entry, uintptr_t user_stack_top);

}

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace x86_64 {

// TODO: 与切换汇编一起定义内核任务的寄存器现场布局。
struct TaskContext;

// TODO: 按 AMD64 调用约定保存当前任务现场并恢复下一任务现场。
extern "C" void switch_to(TaskContext* current, const TaskContext* next);

// 调用前加载 GDT、TSS 和 IDT，并在当前页表中映射用户入口和可写的用户栈。
// 通过 iretq 从 entry 开始执行，使用 user_stack_top 作为 RSP，保持可屏蔽中断关闭。
extern "C" [[noreturn]] void enter_user_mode(uintptr_t entry, uintptr_t user_stack_top);

// 每次启动用户程序时，由调用者持有这份内核返回现场。
struct UserReturnContext {
    uintptr_t kernel_stack_pointer = 0;
};
static_assert(offsetof(UserReturnContext, kernel_stack_pointer) == 0);

// 进入用户态前把原内核栈指针保存到 context。调用条件同 enter_user_mode。
// context 必须存活到 exit_user_mode() 恢复该调用，status 作为本函数的返回值。
extern "C" int run_user_mode(UserReturnContext& context, uintptr_t entry, uintptr_t user_stack_top);

// context 必须来自尚未返回的 run_user_mode() 调用。
extern "C" [[noreturn]] void exit_user_mode(const UserReturnContext& context, int status);

}

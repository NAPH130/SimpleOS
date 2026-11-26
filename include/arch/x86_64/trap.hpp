#pragma once

#include <stdint.h>

namespace x86_64 {

// 描述一次异常的栈上现场，字段从栈顶向高地址排列。
// 栈向低地址增长，汇编最后压入的 r15 位于栈顶，因此是第一个字段。
struct TrapFrame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;        // 系统调用分发器可修改该槽，把结果带回用户态的 RAX。
    uint64_t vector;     // 汇编入口写入的异常编号，供 C++ 分发。
    uint64_t error_code; // 部分异常由 CPU 压入，其余入口由汇编补 0。

    // 以下由 CPU 压栈，供 iretq 恢复原来的执行位置和状态。
    uint64_t rip;        // 故障通常指向出错指令，int3 指向下一条指令。
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;        // 异常发生前的栈指针。
    uint64_t ss;
};

// 注册 CPU 异常与 int 0x80 入口，加载 IDT，并保持可屏蔽中断关闭。
void initialize_traps();

// 由汇编入口调用，frame 指向当前栈上的现场。函数返回后由汇编恢复现场。
extern "C" void trap_dispatch(TrapFrame* frame);

}

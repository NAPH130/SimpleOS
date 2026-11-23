#pragma once

#include <stdint.h>

namespace x86_64 {

// 从栈顶向高地址排列，必须与 trap_entry.S 的压栈顺序一致。
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
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;

    // 以下由 CPU 压栈。
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

// 装载当前仅注册断点向量的 IDT，保持可屏蔽中断关闭。
void initialize_traps();

// 由汇编入口调用，frame 指向本次异常的栈帧。
extern "C" void trap_dispatch(TrapFrame* frame);

}

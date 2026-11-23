#include <arch/x86_64/vectors.h>

    .text
    .macro exception_entry entry, vector, has_error_code=0
    .global \entry
    .type \entry, @function
\entry:
    # CPU 已经保存返回现场。部分异常还自带错误码。
    # 这里补齐缺失的错误码，再压入向量号，让所有入口共用 TrapFrame 布局。
    .if \has_error_code == 0
    pushq $0
    .endif
    pushq $\vector
    jmp .Ltrap_common
    .size \entry, .-\entry
    .endm

#define CPU_EXCEPTION(vector, error, name) exception_entry x86_64_exception_ ## vector, vector, error
#include <arch/x86_64/exception_vectors.inc>
#undef CPU_EXCEPTION

.Ltrap_common:
    # C++ 处理代码可能使用这些寄存器。先保存原值，返回时按反序恢复。
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # System V AMD64 用 rdi 传递第一个参数，这里传入当前 TrapFrame 的地址。
    # rbx 由被调用函数保留，用它记住栈帧位置，便于撤销下面的栈对齐。
    movq %rsp, %rdi
    movq %rsp, %rbx
    # 按调用约定在 call 前对齐到 16 字节，并清除方向标志 DF。
    andq $-16, %rsp
    cld
    call trap_dispatch
    movq %rbx, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    # 向量号和错误码各占 8 字节。跳过它们后，栈顶就是 CPU 保存的 RIP。
    # iretq 从栈上恢复 RIP 和其余返回现场，回到原来的执行流。
    addq $16, %rsp
    iretq

    .section .note.GNU-stack,"",@progbits

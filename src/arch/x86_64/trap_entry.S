#include <arch/x86_64/vectors.h>

    .text
    .macro exception_without_error entry, vector
    .global \entry
    .type \entry, @function
\entry:
    # 为无硬件错误码的异常补齐错误码和向量号。
    pushq $0
    pushq $\vector
    jmp .Ltrap_common
    .size \entry, .-\entry
    .endm

    exception_without_error x86_64_breakpoint_entry, X86_64_VECTOR_BREAKPOINT
    exception_without_error x86_64_invalid_opcode_entry, X86_64_VECTOR_INVALID_OPCODE

.Ltrap_common:
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

    # rdi 传递 TrapFrame，C++ 调用保留 rbx 中的原栈顶。
    movq %rsp, %rdi
    movq %rsp, %rbx
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

    # 跳过向量号和错误码，CPU 恢复余下的返回现场。
    addq $16, %rsp
    iretq

    .section .note.GNU-stack,"",@progbits

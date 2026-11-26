#include <arch/x86_64/segments.h>

    .text
    .global enter_user_mode
    .type enter_user_mode, @function
enter_user_mode:
    # 按 iretq 弹出 RIP、CS、RFLAGS、RSP、SS 的反序构造返回现场。
    # 按 AMD64 调用约定，rdi 是用户入口地址，rsi 是用户栈顶。
    pushq $X86_64_USER_DATA_SELECTOR
    pushq %rsi
    pushq $0x2 # RFLAGS 的保留位 1 为 1，IF 保持为 0。
    pushq $X86_64_USER_CODE_SELECTOR
    pushq %rdi

    # 用户程序从已知的通用寄存器值开始，避免继承内核的临时数据。
    xorl %eax, %eax
    xorl %ebx, %ebx
    xorl %ecx, %ecx
    xorl %edx, %edx
    xorl %ebp, %ebp
    xorl %esi, %esi
    xorl %edi, %edi
    xorl %r8d, %r8d
    xorl %r9d, %r9d
    xorl %r10d, %r10d
    xorl %r11d, %r11d
    xorl %r12d, %r12d
    xorl %r13d, %r13d
    xorl %r14d, %r14d
    xorl %r15d, %r15d
    iretq
    .size enter_user_mode, .-enter_user_mode

    .global run_user_mode
    .type run_user_mode, @function
run_user_mode:
    # 保存 C++ 调用者依赖的寄存器。参数 rdi 和 rsi 直接交给 enter_user_mode。
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, .Lkernel_rsp(%rip)
    jmp enter_user_mode
    .size run_user_mode, .-run_user_mode

    .global exit_user_mode
    .type exit_user_mode, @function
exit_user_mode:
    movq .Lkernel_rsp(%rip), %rdx
    testq %rdx, %rdx
    jz .Lno_user_launch

    # 已在 Ring 0。丢弃这次系统调用的栈帧，恢复原内核栈并返回退出状态。
    movq $0, .Lkernel_rsp(%rip)
    movq %rdx, %rsp
    movw $X86_64_KERNEL_DATA_SELECTOR, %dx
    movw %dx, %ss
    movw %dx, %ds
    movw %dx, %es
    movl %edi, %eax
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
.Lno_user_launch:
    ret
    .size exit_user_mode, .-exit_user_mode

    .bss
    .align 8
.Lkernel_rsp:
    # 该槽在完成退出后清零。首个用户任务与内核共用当前 CPU 和页表。
    .zero 8

    .section .note.GNU-stack,"",@progbits

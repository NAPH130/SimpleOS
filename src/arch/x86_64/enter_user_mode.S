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

    .section .note.GNU-stack,"",@progbits

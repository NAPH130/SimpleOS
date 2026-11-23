#include <arch/x86_64/segments.h>

    .text
    .global x86_64_load_gdt
    .type x86_64_load_gdt, @function
x86_64_load_gdt:
    lgdt (%rdi)
    # lgdt 更新表的位置。远返回用新表重新加载 CS。
    pushq $X86_64_KERNEL_CODE_SELECTOR
    leaq .Lgdt_loaded(%rip), %rax
    pushq %rax
    lretq
.Lgdt_loaded:
    movw $X86_64_KERNEL_DATA_SELECTOR, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs
    movw $X86_64_TSS_SELECTOR, %ax
    # 让 CPU 可通过任务寄存器查到 TSS，随后异常门才能选用其中的 IST。
    ltr %ax
    ret
    .size x86_64_load_gdt, .-x86_64_load_gdt

    .section .note.GNU-stack,"",@progbits

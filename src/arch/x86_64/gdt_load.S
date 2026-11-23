#include <arch/x86_64/segments.h>

    .text
    .global x86_64_load_gdt
    .type x86_64_load_gdt, @function
x86_64_load_gdt:
    lgdt (%rdi)
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
    ltr %ax
    ret
    .size x86_64_load_gdt, .-x86_64_load_gdt

    .section .note.GNU-stack,"",@progbits

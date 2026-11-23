OUTPUT_FORMAT(elf64-x86-64)
ENTRY(syscall_user_main)

SECTIONS {
    /* 将位置无关的入口、用户库及常量放入同一张用户代码页。 */
    . = 0;
    .text : {
        *(.text.entry)
        *(.text .text.*)
        *(.rodata .rodata.*)
    }
    /DISCARD/ : { *(.eh_frame*) *(.note .note.*) *(.got .got.*) }
    ASSERT(SIZEOF(.text) <= 4096, "syscall test user program exceeds one page")
}

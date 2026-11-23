    .section .rodata
    .global syscall_user_begin, syscall_user_end
syscall_user_begin:
    .incbin "syscall_user.bin"
syscall_user_end:

    .section .note.GNU-stack,"",@progbits

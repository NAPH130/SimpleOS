    .section .rodata
    # 导出嵌入内容的边界，内核用它计算需要复制到用户页的字节数。
    .global syscall_user_begin, syscall_user_end
syscall_user_begin:
    .incbin "syscall_user.bin"
syscall_user_end:

    .section .note.GNU-stack,"",@progbits

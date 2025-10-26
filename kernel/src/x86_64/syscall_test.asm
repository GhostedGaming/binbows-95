global call_fb

section .text

call_fb:
    mov rax, 1
    mov rbx, fb_message
    int 0x80
    ret

section .data
    fb_message db "Hello from syscall!", 0
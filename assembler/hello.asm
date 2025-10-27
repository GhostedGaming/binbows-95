.data
    msg: db "Hello from BEXE!", 10, 0

.text
start:
    ; Setup syscall: SYS_FB_PRINT (1) with pointer to msg
    mov rax, 1              ; SYS_FB_PRINT
    mov rbx, msg            ; arg1: pointer to string
    syscall                 ; trigger syscall via interrupt 128
    
    hlt
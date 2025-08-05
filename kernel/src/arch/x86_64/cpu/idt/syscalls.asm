section .text
global syscall_entry

extern syscall_handler

syscall_entry:
    push rbp
    mov rbp, rsp
    call syscall_handler
    pop rbp

    sysretq

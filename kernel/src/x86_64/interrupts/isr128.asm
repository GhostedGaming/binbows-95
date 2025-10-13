global isr128
extern syscall_handler

section .text
isr128:
    ; Common exception stub style: save registers, call syscall_handler, iretq
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov ax, ds
    push rax
    mov ax, es
    push rax

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    mov rbp, rsp
    and rsp, ~0xF

    call syscall_handler

    mov rsp, rbp

    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 0
    iretq

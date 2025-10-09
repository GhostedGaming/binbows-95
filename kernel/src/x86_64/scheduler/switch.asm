global context_switch

section .text

context_switch:
    ; Save current process context onto its stack
    pushfq
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Save current RSP
    mov [rdi], rsp

    ; Load new RSP
    mov rsp, [rsi]

    ; Restore next process context
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
    popfq

    ret
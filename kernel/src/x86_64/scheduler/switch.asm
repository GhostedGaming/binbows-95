global context_switch

section .text

context_switch:
    ; Save current process context onto its stack
    pushfq              ; Save RFLAGS
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

    ; Save current RSP to the address pointed to by RDI
    mov [rdi], rsp

    ; Load new RSP from the address pointed to by RSI
    mov rsp, [rsi]

    ; Restore next process context from its stack
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
    popfq               ; Restore RFLAGS

    ; Return to the next process
    ; For a new process, this will "return" to its entry_point
    ; For a resumed process, this continues after its last context switch
    ret
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
    push r13
    push r14
    push r15

    mov [rdi], rsp
    ; Preserve cr3_arg in r12 (callee-saved) so it survives pops and tests
    mov r12, rdx

    ; Switch to the new stack first
    mov rsp, [rsi]

    test r12, r12
    jz .skip_cr3
    mov rax, r12
    and rax, -2
    mov cr3, rax
.skip_cr3:

    pop r15
    pop r14
    pop r13
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

    test r12, 1
    jne .do_iret
    pop rax
    jmp rax
.do_iret:
    iretq
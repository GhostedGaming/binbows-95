global isr128
extern syscall_handler_c

section .text

isr128:
    ; Save all registers
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
    
    ; Validate syscall number (in rax)
    cmp rax, 255
    jae .invalid
    
    ; Set up arguments for syscall_handler_c
    ; x86-64 calling convention: rdi, rsi, rdx, rcx, r8, r9
    mov rdi, rax    ; arg1: syscall_number
    mov rsi, rbx    ; arg2: arg1
    mov rdx, rcx    ; arg3: arg2
    mov rcx, r8     ; arg4: arg3
    mov r8, r9      ; arg5: arg4
    mov r9, r10     ; arg6: arg5
    
    ; Call the C handler
    call syscall_handler_c
    
    ; rax now contains the return value
    jmp .done
    
.invalid:
    mov rax, -1
    
.done:
    ; Restore all registers (except rax which has return value)
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
    add rsp, 8      ; Skip saved rax
    
    iretq

section .data
max_syscalls: dq 255
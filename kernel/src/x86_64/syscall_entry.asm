; kernel/src/x86_64/syscall_entry.asm
section .text
global syscall_entry

extern syscall_handler_c

syscall_entry:
    ; Save registers
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; Call C handler
    ; Arguments for syscall_handler_c:
    ; rdi = syscall_number (rax)
    ; rsi = arg1 (rdi)
    ; rdx = arg2 (rsi)
    ; rcx = arg3 (rdx)
    ; r8  = arg4 (r10)
    ; r9  = arg5 (r8)

    mov rdi, rax    ; syscall_number
    mov rsi, rbx    ; arg1 (using rbx for first argument from userspace)
    ; ... other arguments if needed

    call syscall_handler_c

    ; Restore registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    ; Return to userspace
    sysretq
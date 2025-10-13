extern isr_handler
extern irq_handler

; ============================================================================
; MACRO DEFINITIONS
; ============================================================================

; Macro for ISRs that don't push an error code (most exceptions)
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        cli                 ; Disable interrupts
        push qword 0        ; Push dummy error code for stack alignment
        push qword %1       ; Push interrupt number
        jmp isr_common_stub
%endmacro

; Macro for ISRs that automatically push an error code (select exceptions)
%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        cli                 ; Disable interrupts
        ; CPU already pushed error code
        push qword %1       ; Push interrupt number
        jmp isr_common_stub
%endmacro

; Macro for IRQ hardware interrupt handlers
%macro IRQ 2
    global irq%1
    irq%1:
        cli                 ; Disable interrupts
        push qword 0        ; Push dummy error code for stack alignment
        push qword %2       ; Push IRQ vector number (32 + IRQ)
        jmp irq_common_stub
%endmacro

; ============================================================================
; CPU EXCEPTION HANDLERS (INT 0-31)
; ============================================================================

ISR_NOERRCODE 0     ; #DE - Division Error
ISR_NOERRCODE 1     ; #DB - Debug Exception
ISR_NOERRCODE 2     ; NMI - Non-Maskable Interrupt
ISR_NOERRCODE 3     ; #BP - Breakpoint
ISR_NOERRCODE 4     ; #OF - Overflow
ISR_NOERRCODE 5     ; #BR - Bound Range Exceeded
ISR_NOERRCODE 6     ; #UD - Invalid Opcode
ISR_NOERRCODE 7     ; #NM - Device Not Available
ISR_ERRCODE   8     ; #DF - Double Fault (has error code)
ISR_NOERRCODE 9     ; Coprocessor Segment Overrun (legacy)
ISR_ERRCODE   10    ; #TS - Invalid TSS (has error code)
ISR_ERRCODE   11    ; #NP - Segment Not Present (has error code)
ISR_ERRCODE   12    ; #SS - Stack-Segment Fault (has error code)
ISR_ERRCODE   13    ; #GP - General Protection Fault (has error code)
ISR_ERRCODE   14    ; #PF - Page Fault (has error code)
ISR_NOERRCODE 15    ; Reserved
ISR_NOERRCODE 16    ; #MF - x87 Floating-Point Exception
ISR_ERRCODE   17    ; #AC - Alignment Check (has error code)
ISR_NOERRCODE 18    ; #MC - Machine Check
ISR_NOERRCODE 19    ; #XM - SIMD Floating-Point Exception
ISR_NOERRCODE 20    ; #VE - Virtualization Exception
ISR_ERRCODE   21    ; #CP - Control Protection Exception (has error code)
ISR_NOERRCODE 22    ; Reserved
ISR_NOERRCODE 23    ; Reserved
ISR_NOERRCODE 24    ; Reserved
ISR_NOERRCODE 25    ; Reserved
ISR_NOERRCODE 26    ; Reserved
ISR_NOERRCODE 27    ; Reserved
ISR_NOERRCODE 28    ; #HV - Hypervisor Injection Exception
ISR_NOERRCODE 29    ; #VC - VMM Communication Exception
ISR_ERRCODE   30    ; #SX - Security Exception (has error code)
ISR_NOERRCODE 31    ; Reserved

; ============================================================================
; HARDWARE IRQ HANDLERS (INT 32+)
; ============================================================================

IRQ 0, 32           ; PIT Timer (IRQ0 -> INT 32)
IRQ 1, 33           ; PS/2 Keyboard (IRQ1 -> INT 33)

; ============================================================================
; COMMON EXCEPTION STUB
; ============================================================================

isr_common_stub:
    ; Save all general purpose registers (System V ABI)
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
    
    ; Save segment registers
    mov ax, ds
    push rax
    mov ax, es
    push rax
    
    ; Load kernel data segment
    mov ax, 0x10        ; Kernel data segment selector
    mov ds, ax
    mov es, ax
    
    ; Align stack to 16 bytes for System V ABI compliance
    mov rbp, rsp
    and rsp, ~0xF
    
    ; Get interrupt number and RIP from saved stack
    ; Stack layout: [regs...] [ds] [es] [r15-rax] [int_no] [err_code] [rip] [cs] [rflags]
    mov rdi, [rbp + 136]  ; Pass interrupt number as first argument
    mov rsi, [rbp + 144]  ; Pass RIP as second argument

    ; Call C exception handler
    call isr_handler
    
    ; Restore original stack pointer
    mov rsp, rbp
    
    ; Restore segment registers
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ; Restore general purpose registers
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
    
    ; Remove error code and interrupt number from stack
    add rsp, 16
    
    ; Return from interrupt (restores RIP, CS, RFLAGS, RSP, SS)
    iretq

; ============================================================================
; COMMON IRQ STUB
; ============================================================================

irq_common_stub:
    ; Save all general purpose registers
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
    
    ; Save segment registers
    mov ax, ds
    push rax
    mov ax, es
    push rax
    
    ; Load kernel data segment
    mov ax, 0x10        ; Kernel data segment selector
    mov ds, ax
    mov es, ax
    
    ; Align stack to 16 bytes for System V ABI compliance
    mov rbp, rsp
    and rsp, ~0xF
    
    ; Get IRQ number from saved stack
    mov rdi, [rbp + 136]  ; Pass IRQ vector as first argument
    
    ; Call C IRQ handler
    call irq_handler
    
    ; Restore original stack pointer
    mov rsp, rbp
    
    ; Restore segment registers
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ; Restore general purpose registers
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
    
    ; Remove error code and IRQ number from stack
    add rsp, 16
    
    ; Return from interrupt
    sti                 ; Re-enable interrupts
    iretq
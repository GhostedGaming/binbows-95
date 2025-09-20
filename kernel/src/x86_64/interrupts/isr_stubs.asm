; ISR assembly handlers for x86_64
; This file defines the low-level interrupt service routines

[BITS 64]

; External C functions
extern isr_handler
extern irq_handler

; Macro for ISRs that don't push an error code
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        push 0          ; Push dummy error code
        push %1         ; Push interrupt number
        jmp isr_common_stub
%endmacro

; Macro for ISRs that push an error code
%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        push %1         ; Push interrupt number
        jmp isr_common_stub
%endmacro

; Macro for IRQ handlers
%macro IRQ 2
    global irq%1
    irq%1:
        push 0          ; Push dummy error code
        push %2         ; Push IRQ number (32 + IRQ number)
        jmp irq_common_stub
%endmacro

; CPU Exception handlers (0-31)
ISR_NOERRCODE 0   ; Division by zero
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; NMI
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound range exceeded
ISR_NOERRCODE 6   ; Invalid opcode
ISR_NOERRCODE 7   ; Device not available
ISR_ERRCODE 8     ; Double fault
ISR_NOERRCODE 9   ; Coprocessor segment overrun
ISR_ERRCODE 10    ; Invalid TSS
ISR_ERRCODE 11    ; Segment not present
ISR_ERRCODE 12    ; Stack-segment fault
ISR_ERRCODE 13    ; General protection fault
ISR_ERRCODE 14    ; Page fault
ISR_NOERRCODE 15  ; Reserved
ISR_NOERRCODE 16  ; x87 floating-point exception
ISR_ERRCODE 17    ; Alignment check
ISR_NOERRCODE 18  ; Machine check
ISR_NOERRCODE 19  ; SIMD floating-point exception
ISR_NOERRCODE 20  ; Virtualization exception
ISR_ERRCODE 21    ; Control protection exception
ISR_NOERRCODE 22  ; Reserved
ISR_NOERRCODE 23  ; Reserved
ISR_NOERRCODE 24  ; Reserved
ISR_NOERRCODE 25  ; Reserved
ISR_NOERRCODE 26  ; Reserved
ISR_NOERRCODE 27  ; Reserved
ISR_NOERRCODE 28  ; Reserved
ISR_NOERRCODE 29  ; Reserved
ISR_ERRCODE 30    ; Security exception
ISR_NOERRCODE 31  ; Reserved

; Hardware IRQ handlers
IRQ 0, 32    ; Timer (IRQ0 -> INT 32)
IRQ 1, 33    ; Keyboard (IRQ1 -> INT 33)

; Common ISR stub for CPU exceptions
isr_common_stub:
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
    
    ; Call C handler with interrupt number as parameter
    mov rdi, [rsp + 120]    ; Get interrupt number from stack
    call isr_handler
    
    ; Restore all registers
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
    
    ; Clean up error code and interrupt number
    add rsp, 16
    
    ; Return from interrupt
    iretq

; Common IRQ stub for hardware interrupts
irq_common_stub:
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
    
    ; Call C handler with IRQ number as parameter
    mov rdi, [rsp + 120]    ; Get IRQ number from stack
    call irq_handler
    
    ; Restore all registers
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
    
    ; Clean up error code and interrupt number
    add rsp, 16
    
    ; Return from interrupt
    iretq
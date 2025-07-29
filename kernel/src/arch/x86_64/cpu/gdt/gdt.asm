global gdt_flush

section .text

gdt_flush:
    ; Parameters:
    ; rdi = pointer to GDT pointer structure
    ; rsi = code segment selector (0x08)
    ; rdx = data segment selector (0x10)
    
    ; Load the GDT
    lgdt [rdi]
    
    ; Save data segment selector
    mov ax, dx
    
    ; Load data segments
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Far jump to reload CS
    ; We need to do a far return to set CS
    push rsi        ; Push code segment
    lea rax, [rel .reload_cs]
    push rax        ; Push return address
    retfq           ; Far return to reload CS
    
.reload_cs:
    ret
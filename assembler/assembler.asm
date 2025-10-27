.data

; Opcodes
OP_MV       :     db 0x01
OP_AD       :     db 0x02
OP_SU       :     db 0x03
OP_MU       :     db 0x04
OP_DI       :     db 0x05
OP_JMP      :     db 0x06
OP_JE       :     db 0x07
OP_JNE      :     db 0x08
OP_JL       :     db 0x09
OP_JG       :     db 0x0A
OP_JLE      :     db 0x0B
OP_JGE      :     db 0x0C
OP_CMP      :     db 0x10
OP_PUSH     :     db 0x20
OP_POP      :     db 0x21
OP_CALL     :     db 0x30
OP_RET      :     db 0x31
OP_HLT      :     db 0x40
OP_SYSCALL  :     db 0x50
OP_NOP      :     db 0x90

; Registers
REG_RAX     :     db 0x00
REG_RBX     :     db 0x01
REG_RCX     :     db 0x02
REG_RDX     :     db 0x03
REG_RSI     :     db 0x04
REG_RDI     :     db 0x05
REG_RSP     :     db 0x06
REG_RBP     :     db 0x07
REG_R8      :     db 0x08
REG_R9      :     db 0x09
REG_R10     :     db 0x0A
REG_R11     :     db 0x0B
REG_R12     :     db 0x0C
REG_R13     :     db 0x0D
REG_R14     :     db 0x0E
REG_R15     :     db 0x0F

; Syscalls
SYS_RESERVED_CODE      :    db 0
SYS_FB_PRINT_CODE      :    db 1
SYS_READ_FILE_CODE     :    db 2
SYS_WRITE_FILE_CODE    :    db 3
SYS_OPEN_FILE_CODE     :    db 4
SYS_CLOSE_FILE_CODE    :    db 5
SYS_KMALLOC_CODE       :    db 6
SYS_KFREE_CODE         :    db 7
SYS_MEMCPY_CODE        :    db 8
SYS_MEMCMP_CODE        :    db 9
SYS_MEMSET_CODE        :    db 10

; Messages
error_msg       :   db "Usage: assembler <input_file> [drive]", 0x0A, 0
success_msg     :   db "File read successfully!", 0x0A, 0
read_error_msg  :   db "Error: Could not read file", 0x0A, 0
mv_msg          :   db "Parsed mv", 0x0A, 0
ad_msg          :   db "Parsed ad", 0x0A, 0
su_msg          :   db "Parsed su", 0x0A, 0

.text
start:
    ; Setup stack pointer and get argc/argv
    pop rax
    mov rbx, rsp

    cmp rax, 2
    jl no_args

    mov r15, 0
    cmp rax, 3
    jl use_default_drive

    mov rcx, [rbx + 16]      ; argv[2] (drive)
    mov r15, byte [rcx]
    sub r15, 0x30

use_default_drive:
    mov rcx, [rbx + 8]       ; argv[1] (file path)
    push 0
    push 0

    mov rax, SYS_READ_FILE_CODE
    mov rdi, r15
    mov rsi, rcx
    mov rdx, rsp
    mov r8, rsp
    add r8, 8
    syscall

    cmp rax, 0
    jne read_error

    pop r9
    pop r10

    mov rax, SYS_FB_PRINT_CODE
    mov rdi, success_msg
    syscall

    call tokenize

    mov rax, SYS_KFREE_CODE
    mov rdi, r9
    syscall

    hlt

no_args:
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, error_msg
    syscall
    hlt

read_error:
    pop r9
    pop r10
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, read_error_msg
    syscall
    hlt

; -------------------------
; Tokenizer
; -------------------------
tokenize:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15

    mov r12, r9          ; current ptr
    mov r13, r9          ; start of line
    xor r14, r14         ; line counter
    mov r15, r10         ; end ptr

tokenize_loop:
    cmp r12, r15
    jge tokenize_done

    mov al, [r12]
    cmp al, 0x0A
    je found_newline

    inc r12
    jmp tokenize_loop

found_newline:
    push r9
    push r10

    mov rdi, r13
    mov rsi, r12
    sub rsi, r13
    call process_line

    pop r10
    pop r9

    inc r14
    inc r12
    mov r13, r12
    jmp tokenize_loop

tokenize_done:
    cmp r13, r15
    jge tokenize_cleanup

    push r9
    push r10
    mov rdi, r13
    mov rsi, r15
    sub rsi, r13
    call process_line
    pop r10
    pop r9

tokenize_cleanup:
    pop r15
    pop r14
    pop r13
    pop r12
    mov rsp, rbp
    pop rbp
    ret

; -------------------------
; Process one line
; -------------------------
process_line:
    push rbp
    mov rbp, rsp
    cmp rsi, 0
    je process_line_done

    push rdi
    push rsi

    ; Allocate memory
    mov rax, SYS_KMALLOC_CODE
    mov rdi, rsi
    add rdi, 1
    syscall
    mov rbx, rax

    pop rsi
    pop rdi

    ; Copy line into allocated buffer
    push rdi
    push rsi
    mov rax, SYS_MEMCPY_CODE
    mov rdi, rbx
    mov rdx, rsi
    syscall
    pop rsi
    pop rdi

    ; Null-terminate
    mov byte [rbx + rsi], 0

    ; Parse instruction
    mov rcx, rbx
    cmp byte [rcx], 'm'
    jne check_ad
    cmp byte [rcx + 1], 'v'
    jne check_ad
    ; mv found
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, mv_msg
    syscall
    jmp parse_done

check_ad:
    cmp byte [rcx], 'a'
    jne check_su
    cmp byte [rcx + 1], 'd'
    jne check_su
    ; ad found
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, ad_msg
    syscall
    jmp parse_done

check_su:
    cmp byte [rcx], 's'
    jne parse_done
    cmp byte [rcx + 1], 'u'
    jne parse_done
    ; su found
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, su_msg
    syscall

parse_done:
    mov rax, SYS_KFREE_CODE
    mov rdi, rbx
    syscall

process_line_done:
    mov rsp, rbp
    pop rbp
    ret

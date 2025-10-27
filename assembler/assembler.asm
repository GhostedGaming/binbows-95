.data

; Opcodes (same as before)
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
parse_ok_msg    :   db "Parsed line OK", 0x0A, 0
parse_skip_msg  :   db "Skipped/empty line", 0x0A, 0
doc_reco_msg    :   db "Recommendation: Use this parser in docs; outputs instruction,type,operand1,operand2", 0x0A, 0

; small buffers for parse output (on heap we use kmalloc; static scratch used below for short messages)
scratch_msg     :   db "parse:", 0x20, 0

.text
start:
    pop rax                ; argc (popped)
    mov rbx, rsp           ; argv pointer

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

    ; kernel returns pointers in r9 (buf) and r10 (end)
    pop r9
    pop r10

    mov rax, SYS_FB_PRINT_CODE
    mov rdi, success_msg
    syscall

    call tokenize_all

    mov rax, SYS_KFREE_CODE
    mov rdi, r9
    syscall

    ; Print recommendation message for docs
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, doc_reco_msg
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
; Tokenize and parse everything
; -------------------------
tokenize_all:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15

    mov r12, r9          ; current ptr
    mov r13, r9          ; start of line
    xor r14, r14         ; line count
    mov r15, r10         ; end ptr

.tok_loop:
    cmp r12, r15
    jge .tok_done

    mov al, [r12]
    cmp al, 0x0A
    je .got_eol

    inc r12
    jmp .tok_loop

.got_eol:
    ; call parse_line(r13, r12 - r13)
    push r12
    push r15

    mov rdi, r13
    mov rsi, r12
    sub rsi, r13
    call parse_line_full

    pop r15
    pop r12

    inc r14
    inc r12
    mov r13, r12
    jmp .tok_loop

.tok_done:
    cmp r13, r15
    jge .tok_cleanup

    ; remaining partial line
    push r12
    push r15

    mov rdi, r13
    mov rsi, r15
    sub rsi, r13
    call parse_line_full

    pop r15
    pop r12

.tok_cleanup:
    pop r15
    pop r14
    pop r13
    pop r12
    mov rsp, rbp
    pop rbp
    ret

; -------------------------
; parse_line_full
; parameters:
;   rdi = pointer to start of line
;   rsi = length of line (bytes)
; Produces: a small parse buffer of form:
;    "<instr> <type> <op1_repr> <op2_repr>\n"
; Writes it to FB via SYS_FB_PRINT_CODE (via allocated buffer)
; -------------------------
parse_line_full:
    push rbp
    mov rbp, rsp

    ; quick empty/whitespace-only check
    cmp rsi, 0
    je .plf_done

    push rbx
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11

    mov rbx, rdi        ; src
    mov rcx, rsi        ; len

    ; allocate a local copy (len + 1)
    mov rax, SYS_KMALLOC_CODE
    mov rdi, rcx
    add rdi, 1
    syscall
    mov r11, rax        ; allocated buffer (null-terminated copy)

    ; memcpy src -> r11, length rcx
    mov rax, SYS_MEMCPY_CODE
    mov rdi, r11
    mov rsi, rbx
    mov rdx, rcx
    syscall

    mov byte [r11 + rcx], 0    ; null terminate

    ; Trim leading spaces/tabs
    mov rdx, r11
.trim_lead:
    mov al, [rdx]
    cmp al, ' '
    je .do_trim_lead_skip
    cmp al, 0x09
    je .do_trim_lead_skip
    jmp .trim_done
.do_trim_lead_skip:
    inc rdx
    jmp .trim_lead
.trim_done:

    ; If empty after trim => skip
    mov al, [rdx]
    cmp al, 0
    je .plf_skip

    ; parse instruction token: read letters until space, tab, or end
    mov r8, rdx      ; instr start
    xor r9, r9       ; instr length counter
.instr_collect:
    mov al, [r8 + r9]
    cmp al, 0
    je .instr_col_done
    cmp al, ' '
    je .instr_col_done
    cmp al, 0x09
    je .instr_col_done
    ; accept ascii letters (a-z, A-Z) and punctuation used in mnemonics
    inc r9
    jmp .instr_collect
.instr_col_done:

    cmp r9, 0
    je .plf_skip

    ; Lower-case normalize: For simplicity assume input lower-case; otherwise convert per byte
    ; Build instruction name into small allocated instr buffer
    mov rax, SYS_KMALLOC_CODE
    mov rdi, r9
    add rdi, 1
    syscall
    mov r10, rax    ; instr_buf

    ; copy instr bytes
    mov rsi, r8
    mov rdi, r10
    mov rdx, r9
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [r10 + r9], 0

    ; move pointer after instr token
    add r8, r9
    ; skip spaces/tabs
.skip_ws_after_instr:
    mov al, [r8]
    cmp al, ' '
    je .skip_ws_incr
    cmp al, 0x09
    je .skip_ws_incr
    jmp .operands_start
.skip_ws_incr:
    inc r8
    jmp .skip_ws_after_instr

.operands_start:
    ; now r8 points at first operand char or end
    ; Parse up to two operands separated by comma.
    ; operand parsing writes a small representation string for each operand
    ; We'll allocate small buffers for op1 and op2 (max len = remaining bytes + 1)
    mov rax, SYS_KMALLOC_CODE
    mov rdi, rcx
    add rdi, 1
    syscall
    mov r12, rax    ; op1_buf

    mov rax, SYS_KMALLOC_CODE
    mov rdi, rcx
    add rdi, 1
    syscall
    mov r13, rax    ; op2_buf

    ; initialize op buffers to empty
    mov byte [r12], 0
    mov byte [r13], 0

    ; parse operand 1
    mov rsi, r8
    ; find comma or end
    xor r14, r14
.find_sep1:
    mov al, [rsi + r14]
    cmp al, 0
    je .sep1_done
    cmp al, ','
    je .sep1_done
    inc r14
    jmp .find_sep1
.sep1_done:
    ; trim trailing spaces from operand 1 region
    mov rdx, r14
    cmp rdx, 0
    je .op1_empty
    dec rdx
.trim_op1_trail:
    mov al, [rsi + rdx]
    cmp al, ' '
    je .do_trim_op1_dec
    cmp al, 0x09
    je .do_trim_op1_dec
    jmp .op1_extract
.do_trim_op1_dec:
    dec rdx
    cmp rdx, 0xFFFFFFFFFFFFFFFF
    jne .trim_op1_trail
    jmp .op1_extract
.op1_extract:
    ; rsi..rsi+r14-1 is the raw operand1; rdx currently points to last non-space offset (index)
    mov rcx, rdx
    add rcx, 1
    mov rdi, r12
    mov rsi, rsi
    mov rdx, rcx
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [r12 + rcx], 0
    jmp .op1_done
.op1_empty:
    ; no operand1
    mov byte [r12], 0
.op1_done:

    ; if there was comma, set r8 to char after comma for operand2
    mov al, [rsi + r14]
    cmp al, ','
    jne .no_op2
    ; move to char after comma
    lea r8, [rsi + r14 + 1]
    ; skip leading spaces for op2
    mov rsi, r8
.skip_lead_op2:
    mov al, [rsi]
    cmp al, ' '
    je .skip_lead_inc
    cmp al, 0x09
    je .skip_lead_inc
    jmp .op2_collect_start
.skip_lead_inc:
    inc rsi
    jmp .skip_lead_op2

.op2_collect_start:
    ; collect until end or newline
    mov rdx, 0
.collect_op2_loop:
    mov al, [rsi + rdx]
    cmp al, 0
    je .collect_op2_done
    ; accept until newline or null
    inc rdx
    jmp .collect_op2_loop
.collect_op2_done:
    cmp rdx, 0
    je .op2_empty
    mov rax, SYS_MEMCPY_CODE
    mov rdi, r13
    mov rsi, rsi
    mov rdx, rdx
    syscall
    mov byte [r13 + rdx], 0
    jmp .op2_done
.op2_empty:
    mov byte [r13], 0
.op2_done:
    jmp .parse_operands_done
.no_op2:
    ; nothing to do, op2 empty already
.parse_operands_done:

    ; Normalize op buffers and figure out types; create human-friendly repr in-place
    ; op type detection rules (simple):
    ; - If starts with '[' and ends with ']' -> memory, register inside
    ; - If starts with letter then letters/digits -> register (match known regs)
    ; - If starts with '0x' or digit or '-' -> immediate (hex or decimal)
    ; We'll produce strings like: "reg:RAX", "imm:123", "mem:[RBX]"

    ; helper: process op in r12 (op1)
    mov rdi, r12
    call classify_and_normalize_operand
    ; result: rax -> pointer to normalized string (stays r12)

    ; helper: process op in r13 (op2)
    mov rdi, r13
    call classify_and_normalize_operand

    ; build final parse line: "<instr> <op1> <op2>\n"
    ; compute lengths
    ; instr in r10, op1 in r12, op2 in r13
    ; compute instr len
    xor rax, rax
    mov rsi, r10
.find_len_instr:
    mov bl, [rsi + rax]
    cmp bl, 0
    je .len_instr_done
    inc rax
    jmp .find_len_instr
.len_instr_done:
    mov r14, rax  ; instr_len

    ; op1 len
    xor rax, rax
    mov rsi, r12
.find_len_op1:
    mov bl, [rsi + rax]
    cmp bl, 0
    je .len_op1_done
    inc rax
    jmp .find_len_op1
.len_op1_done:
    mov r15, rax

    ; op2 len
    xor rax, rax
    mov rsi, r13
.find_len_op2:
    mov bl, [rsi + rax]
    cmp bl, 0
    je .len_op2_done
    inc rax
    jmp .find_len_op2
.len_op2_done:
    mov r9, rax

    ; total len = instr_len + 1 + op1_len + 1 + op2_len + 1
    mov rax, r14
    add rax, 1
    add rax, r15
    add rax, 1
    add rax, r9
    add rax, 1
    ; allocate output buffer
    mov rcx, rax
    mov rdi, rcx
    ; reusing SYS_KMALLOC_CODE
    mov rax, SYS_KMALLOC_CODE
    syscall
    mov rbx, rax  ; out_buf

    ; copy instr
    mov rsi, r10
    mov rdx, r14
    mov rdi, rbx
    mov rax, SYS_MEMCPY_CODE
    syscall
    ; add space
    mov byte [rbx + r14], ' '
    ; copy op1
    mov rsi, r12
    mov rdx, r15
    mov rdi, rbx
    add rdi, r14
    add rdi, 1
    mov rax, SYS_MEMCPY_CODE
    syscall
    ; add space after op1 (even if op1 empty we'll put a space)
    mov byte [rbx + r14 + 1 + r15], ' '
    ; copy op2
    mov rsi, r13
    mov rdx, r9
    mov rdi, rbx
    add rdi, r14
    add rdi, 1
    add rdi, r15
    add rdi, 1
    mov rax, SYS_MEMCPY_CODE
    syscall
    ; add newline
    mov byte [rbx + r14 + 1 + r15 + 1 + r9], 0x0A
    ; terminate after newline (optional)
    mov byte [rbx + r14 + 1 + r15 + 1 + r9 + 1], 0

    ; print the parse result
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, rbx
    syscall

    ; free out buffer
    mov rax, SYS_KFREE_CODE
    mov rdi, rbx
    syscall

    ; free instr, op1, op2, copy buffer
    mov rax, SYS_KFREE_CODE
    mov rdi, r10
    syscall
    mov rax, SYS_KFREE_CODE
    mov rdi, r12
    syscall
    mov rax, SYS_KFREE_CODE
    mov rdi, r13
    syscall
    mov rax, SYS_KFREE_CODE
    mov rdi, r11
    syscall

    ; small success message (optional)
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, parse_ok_msg
    syscall

    jmp .plf_done_restore

.plf_skip:
    ; free copied buffer
    mov rax, SYS_KFREE_CODE
    mov rdi, r11
    syscall
    mov rax, SYS_FB_PRINT_CODE
    mov rdi, parse_skip_msg
    syscall
    jmp .plf_done_restore

.plf_done_restore:
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rbx

.plf_done:
    mov rsp, rbp
    pop rbp
    ret

; -------------------------
; classify_and_normalize_operand
; Input:
;   rdi -> pointer to a null-terminated operand string
; Output:
;   rax -> same pointer (rdi) after normalization (string modified in-place)
; Effects:
;   - replaces operand string with "reg:NAME" or "imm:VAL" or "mem:[REG]"
; Notes: uses small tables for known register names (lower-case expected)
; -------------------------
classify_and_normalize_operand:
    push rbp
    mov rbp, rsp
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi

    mov rsi, rdi      ; operand ptr
    mov al, [rsi]
    cmp al, 0
    je .ret_empty

    ; skip leading spaces
    xor rcx, rcx
.skip_lead:
    mov al, [rsi + rcx]
    cmp al, ' '
    je .do_skip_lead_inc
    cmp al, 0x09
    je .do_skip_lead_inc
    jmp .post_lead
.do_skip_lead_inc:
    inc rcx
    jmp .skip_lead
.post_lead:
    add rsi, rcx

    ; check first char
    mov al, [rsi]
    cmp al, '['
    je .is_memory
    cmp al, '-'
    je .is_immediate
    cmp al, '0'
    jb .maybe_reg   ; chars < '0' likely letters (register)
    cmp al, '9'
    jbe .is_immediate ; starts with digit -> immediate
    cmp al, 'a'
    jb .maybe_reg
    ; letter -> register or label
    jmp .maybe_reg

.is_memory:
    ; memory form: [reg] or [reg+offset] (we handle simple [reg])
    ; find closing ']'
    mov rcx, 1
.find_cl_br:
    mov al, [rsi + rcx]
    cmp al, 0
    je .mem_fallback
    cmp al, ']'
    je .mem_extract
    inc rcx
    jmp .find_cl_br
.mem_extract:
    ; copy inside content to temp (rsi +1, length rcx-1)
    mov rdx, rcx
    dec rdx
    ; create small temp buffer
    mov rax, SYS_KMALLOC_CODE
    mov rdi, rdx
    add rdi, 1
    syscall
    mov rbx, rax
    mov rdi, rbx
    mov rsi, rsi
    add rsi, 1
    mov rdx, rdx
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [rbx + rdx], 0

    ; attempt register match on rbx
    mov rdi, rbx
    call detect_register_name
    cmp rax, -1
    je .mem_fallback2
    ; rax = reg_id (0..15), also we want printable canonical name
    ; build "mem:[REGNAME]" into original operand ptr location
    ; For simplicity build string using known names in code: "mem:[raxname]"
    ; We'll allocate a small buffer for normalized form
    mov rdx, 16
    mov rax, SYS_KMALLOC_CODE
    mov rdi, rdx
    syscall
    mov rcx, rax    ; norm_buf
    ; default prefix "mem:["
    mov byte [rcx + 0], 'm'
    mov byte [rcx + 1], 'e'
    mov byte [rcx + 2], 'm'
    mov byte [rcx + 3], ':'
    mov byte [rcx + 4], '['
    ; choose reg name
    cmp rax, 0
    ; We'll map reg id later; but easier: use detect_register_name to also return pointer to canonical name.
    ; Instead, free norm_buf and build using a helper that returns canonical reg name pointer.
    mov rax, SYS_KFREE_CODE
    mov rdi, rcx
    syscall

    ; get canonical reg name pointer in rbx2 via helper
    mov rdi, rbx
    call canonical_reg_name_ptr
    mov rcx, rax    ; rcx -> name ptr

    ; Build final normalized string into original operand buffer (overwrite)
    mov rsi, rdi    ; original operand pointer passed in on stack (saved earlier)
    ; compute lengths
    xor rax, rax
.len_cname:
    mov dl, [rcx + rax]
    cmp dl, 0
    je .lname_done
    inc rax
    jmp .len_cname
.lname_done:
    mov rdx, rax

    ; total len = 4 + rdx + 2 = rdx + 6  (mem:[NAME])
    mov rax, rdx
    add rax, 6
    ; allocate temp output
    mov rdi, rax
    mov rax, SYS_KMALLOC_CODE
    syscall
    mov r8, rax

    ; write "mem:["
    mov byte [r8 + 0], 'm'
    mov byte [r8 + 1], 'e'
    mov byte [r8 + 2], 'm'
    mov byte [r8 + 3], ':'
    mov byte [r8 + 4], '['
    ; copy name
    mov rsi, rcx
    mov rdi, r8
    add rdi, 5
    mov rdx, rdx
    mov rax, SYS_MEMCPY_CODE
    syscall
    ; write closing ]
    mov byte [r8 + 5 + rdx], ']'
    mov byte [r8 + 5 + rdx + 1], 0

    ; replace original operand string (copy r8 into rdi)
    mov rsi, r8
    ; compute total len (rdx + 6)
    mov rdx, rdx
    add rdx, 6
    mov rdi, rdi  ; operand original pointer (on stack)
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [rdi + rdx], 0

    ; free temp buffers
    mov rax, SYS_KFREE_CODE
    mov rdi, rbx
    syscall
    mov rax, SYS_KFREE_CODE
    mov rdi, r8
    syscall

    ; return original pointer in rax (operand pointer)
    mov rax, rdi
    jmp .class_done

.mem_fallback:
    ; couldn't parse mem, treat as raw string -> mark as 'mem:UNK'
    mov rax, SYS_KMALLOC_CODE
    mov rdi, 8
    syscall
    mov r8, rax
    mov byte [r8], 'm'
    mov byte [r8 + 1], 'e'
    mov byte [r8 + 2], 'm'
    mov byte [r8 + 3], ':'
    mov byte [r8 + 4], '['
    mov byte [r8 + 5], '?'
    mov byte [r8 + 6], ']'
    mov byte [r8 + 7], 0
    ; copy back
    mov rsi, r8
    mov rdi, rdi
    mov rdx, 7
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [rdi + 7], 0
    mov rax, rdi
    mov rdi, r8
    mov rax, SYS_KFREE_CODE
    syscall
    jmp .class_done

.mem_fallback2:
    ; on failure (no reg matched), same fallback
    jmp .mem_fallback

.is_immediate:
    ; immediate: copy as "imm:VALUE" (trim spaces)
    ; compute length
    mov rcx, 0
.immed_len:
    mov al, [rsi + rcx]
    cmp al, 0
    je .immed_len_done
    inc rcx
    jmp .immed_len
.immed_len_done:
    ; allocate out = "imm:" + rcx + 1
    mov rax, rcx
    add rax, 5
    mov rdi, rax
    mov rax, SYS_KMALLOC_CODE
    syscall
    mov rbx, rax
    mov byte [rbx + 0], 'i'
    mov byte [rbx + 1], 'm'
    mov byte [rbx + 2], 'm'
    mov byte [rbx + 3], ':'
    ; copy value
    mov rsi, rsi
    mov rdi, rbx
    add rdi, 4
    mov rdx, rcx
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [rbx + 4 + rcx], 0
    ; copy back to operand pointer
    mov rdi, rdi        ; original operand ptr (on stack)
    sub rdi, 4
    ; careful: original operand pointer is in [rbp+?], we saved earlier; but to simplify,
    ; use the preserved pointer from the stack: it was pushed at function entry. We can read it:
    mov rdi, [rbp + 16] ; original rdi saved by caller in push rdi
    ; copy normalized string back
    mov rsi, rbx
    ; compute total len
    mov rdx, rcx
    add rdx, 5
    mov rax, SYS_MEMCPY_CODE
    mov rdx, rdx
    mov rax, SYS_MEMCPY_CODE
    mov rdi, rdi
    syscall
    mov byte [rdi + rdx], 0
    mov rax, rdi
    mov rdi, rbx
    mov rax, SYS_KFREE_CODE
    syscall
    jmp .class_done

.maybe_reg:
    ; try to detect register name; we assume lower-case names like rax, rbx, r8 etc.
    mov rdx, 0
.reg_len_loop:
    mov al, [rsi + rdx]
    cmp al, 0
    je .reg_len_done
    cmp al, ' '
    je .reg_len_done
    cmp al, ','
    je .reg_len_done
    inc rdx
    jmp .reg_len_loop
.reg_len_done:
    ; copy into temp
    mov rax, SYS_KMALLOC_CODE
    mov rdi, rdx
    add rdi, 1
    syscall
    mov rbx, rax
    mov rdi, rbx
    mov rsi, rsi
    mov rdx, rdx
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [rbx + rdx], 0

    mov rdi, rbx
    call detect_register_name
    cmp rax, -1
    je .not_reg

    ; reg matched: build "reg:NAME"
    mov rax, SYS_KMALLOC_CODE
    mov rdi, 8
    syscall
    mov rcx, rax
    mov byte [rcx + 0], 'r'
    mov byte [rcx + 1], 'e'
    mov byte [rcx + 2], 'g'
    mov byte [rcx + 3], ':'
    ; get canonical name pointer
    mov rdi, rbx
    call canonical_reg_name_ptr
    mov rsi, rax
    ; copy reg name
    ; compute name len
    xor rax, rax
.name_len_loop:
    mov dl, [rsi + rax]
    cmp dl, 0
    je .name_len_done
    inc rax
    jmp .name_len_loop
.name_len_done:
    mov rdx, rax
    mov rdi, rcx
    add rdi, 4
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [rcx + 4 + rdx], 0
    ; copy back into operand original pointer (saved earlier)
    mov rdi, [rbp + 16]   ; original operand pointer
    mov rsi, rcx
    mov rax, SYS_MEMCPY_CODE
    mov rdx, rdx
    add rdx, 5
    syscall
    mov byte [rdi + rdx], 0
    ; free temps
    mov rax, SYS_KFREE_CODE
    mov rdi, rbx
    syscall
    mov rax, SYS_KFREE_CODE
    mov rdi, rcx
    syscall
    mov rax, rdi
    jmp .class_done

.not_reg:
    ; fallback: mark as label/identifier -> "lbl:NAME"
    mov rax, SYS_KMALLOC_CODE
    mov rdi, rdx
    add rdi, 5
    syscall
    mov rbx, rax
    mov byte [rbx + 0], 'l'
    mov byte [rbx + 1], 'b'
    mov byte [rbx + 2], 'l'
    mov byte [rbx + 3], ':'
    ; copy name
    mov rsi, rsi
    mov rdi, rbx
    add rdi, 4
    mov rdx, rdx
    mov rax, SYS_MEMCPY_CODE
    syscall
    mov byte [rbx + 4 + rdx], 0
    ; copy back
    mov rdi, [rbp + 16]
    mov rsi, rbx
    mov rax, SYS_MEMCPY_CODE
    mov rdx, rdx
    add rdx, 4
    syscall
    mov byte [rdi + rdx], 0
    mov rax, rdi
    mov rax, SYS_KFREE_CODE
    mov rdi, rbx
    syscall
    mov rax, rdi
    jmp .class_done

.ret_empty:
    mov rax, rdi
    jmp .class_done

.class_done:
    ; restore and return
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; -------------------------
; detect_register_name
; Input: rdi -> pointer to null-terminated candidate string (lower-case)
; Output:
;   rax = reg id (0..15) on success
;   rax = -1 on failure
; -------------------------
detect_register_name:
    push rbp
    mov rbp, rsp
    push rbx
    push rcx

    mov rsi, rdi
    ; quick checks for common registers
    ; compare with "rax","rbx","rcx","rdx","rsi","rdi","rsp","rbp"
    ; for r8..r15 check "r8","r9", "r10" etc.

    ; check rax
    mov byte [rbp-1], 0
    mov rcx, rsi
    ; compare strings manually
    mov rax, 0
    ; we'll implement simple branching checks

    ; compare with "rax"
    mov rdx, 0
    mov al, [rsi + rdx]
    cmp al, 'r'
    jne .chk_r8
    mov al, [rsi + rdx + 1]
    cmp al, 'a'
    jne .chk_r8
    mov al, [rsi + rdx + 2]
    cmp al, 'x'
    jne .chk_r8
    mov al, [rsi + rdx + 3]
    cmp al, 0
    jne .chk_r8
    mov rax, 0
    jmp .dr_done

.chk_r8:
    ; rbx
    mov al, [rsi]
    cmp al, 'r'
    jne .chk_rcx
    mov al, [rsi + 1]
    cmp al, 'b'
    jne .chk_rcx
    mov al, [rsi + 2]
    cmp al, 'x'
    jne .chk_rcx
    mov al, [rsi + 3]
    cmp al, 0
    jne .chk_rcx
    mov rax, 1
    jmp .dr_done

.chk_rcx:
    mov al, [rsi]
    cmp al, 'r'
    jne .chk_rdx
    mov al, [rsi + 1]
    cmp al, 'c'
    jne .chk_rdx
    mov al, [rsi + 2]
    cmp al, 'x'
    jne .chk_rdx
    mov al, [rsi + 3]
    cmp al, 0
    jne .chk_rdx
    mov rax, 2
    jmp .dr_done

.chk_rdx:
    mov al, [rsi]
    cmp al, 'r'
    jne .chk_rsi
    mov al, [rsi + 1]
    cmp al, 'd'
    jne .chk_rsi
    mov al, [rsi + 2]
    cmp al, 'x'
    jne .chk_rsi
    mov al, [rsi + 3]
    cmp al, 0
    jne .chk_rsi
    mov rax, 3
    jmp .dr_done

.chk_rsi:
    mov al, [rsi]
    cmp al, 'r'
    jne .chk_rdi
    mov al, [rsi + 1]
    cmp al, 's'
    jne .chk_rdi
    mov al, [rsi + 2]
    cmp al, 'i'
    jne .chk_rdi
    mov al, [rsi + 3]
    cmp al, 0
    jne .chk_rdi
    mov rax, 4
    jmp .dr_done

.chk_rdi:
    mov al, [rsi]
    cmp al, 'r'
    jne .chk_rsp
    mov al, [rsi + 1]
    cmp al, 'd'
    jne .chk_rsp
    mov al, [rsi + 2]
    cmp al, 'i'
    jne .chk_rsp
    mov al, [rsi + 3]
    cmp al, 0
    jne .chk_rsp
    mov rax, 5
    jmp .dr_done

.chk_rsp:
    mov al, [rsi]
    cmp al, 'r'
    jne .chk_rbp
    mov al, [rsi + 1]
    cmp al, 's'
    jne .chk_rbp
    mov al, [rsi + 2]
    cmp al, 'p'
    jne .chk_rbp
    mov al, [rsi + 3]
    cmp al, 0
    jne .chk_rbp
    mov rax, 6
    jmp .dr_done

.chk_rbp:
    mov al, [rsi]
    cmp al, 'r'
    jne .chk_r8n
    mov al, [rsi + 1]
    cmp al, 'b'
    jne .chk_r8n
    mov al, [rsi + 2]
    cmp al, 'p'
    jne .chk_r8n
    mov al, [rsi + 3]
    cmp al, 0
    jne .chk_r8n
    mov rax, 7
    jmp .dr_done

.chk_r8n:
    ; r8 - r15: check starting with 'r' followed by digit
    mov al, [rsi]
    cmp al, 'r'
    jne .dr_fail
    mov al, [rsi + 1]
    cmp al, '8'
    je .chk_digit_end_1
    cmp al, '9'
    je .chk_digit_end_1
    cmp al, '1'
    jne .dr_fail
    ; could be r10..r15
    mov al, [rsi + 2]
    cmp al, '0'
    je .chk_digit_end_2
    cmp al, '1'
    je .chk_digit_end_2
    cmp al, '2'
    je .chk_digit_end_2
    cmp al, '3'
    je .chk_digit_end_2
    cmp al, '4'
    je .chk_digit_end_2
    cmp al, '5'
    je .chk_digit_end_2
    cmp al, 0
    jne .dr_fail
    jmp .dr_fail

.chk_digit_end_1:
    ; r8 or r9 - expect null-terminator at +2
    mov al, [rsi + 2]
    cmp al, 0
    jne .dr_fail
    mov al, [rsi + 1]
    cmp al, '8'
    je .set_r8
    cmp al, '9'
    je .set_r9
    jmp .dr_fail

.set_r8:
    mov rax, 8
    jmp .dr_done
.set_r9:
    mov rax, 9
    jmp .dr_done

.chk_digit_end_2:
    ; r10..r15: expect null at +3
    mov al, [rsi + 3]
    cmp al, 0
    jne .dr_fail
    mov al, [rsi + 1]
    cmp al, '1'
    jne .dr_fail
    mov al, [rsi + 2]
    cmp al, '0'
    je .set_r10
    cmp al, '1'
    je .set_r11
    cmp al, '2'
    je .set_r12
    cmp al, '3'
    je .set_r13
    cmp al, '4'
    je .set_r14
    cmp al, '5'
    je .set_r15
    jmp .dr_fail

.set_r10:
    mov rax, 10
    jmp .dr_done
.set_r11:
    mov rax, 11
    jmp .dr_done
.set_r12:
    mov rax, 12
    jmp .dr_done
.set_r13:
    mov rax, 13
    jmp .dr_done
.set_r14:
    mov rax, 14
    jmp .dr_done
.set_r15:
    mov rax, 15
    jmp .dr_done

.dr_fail:
    mov rax, -1
.dr_done:
    pop rcx
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; -------------------------
; canonical_reg_name_ptr
; Input: rdi -> pointer to candidate reg-string (null-term)
; Output: rax -> pointer to canonical name (e.g., "RAX", "RBX") allocated via kmalloc (caller must free)
; -------------------------
canonical_reg_name_ptr:
    push rbp
    mov rbp, rsp
    push rbx
    push rcx

    mov rsi, rdi
    ; find register id via detect_register_name
    mov rdi, rsi
    call detect_register_name
    cmp rax, -1
    je .canon_fail

    mov rbx, rax  ; reg id
    ; allocate small buffer for name (max 4 bytes + 1)
    mov rax, SYS_KMALLOC_CODE
    mov rdi, 5
    syscall
    mov rcx, rax

    ; fill uppercase names
    cmp rbx, 0
    je .name_rax
    cmp rbx, 1
    je .name_rbx
    cmp rbx, 2
    je .name_rcx
    cmp rbx, 3
    je .name_rdx
    cmp rbx, 4
    je .name_rsi
    cmp rbx, 5
    je .name_rdi
    cmp rbx, 6
    je .name_rsp
    cmp rbx, 7
    je .name_rbp
    cmp rbx, 8
    je .name_r8
    cmp rbx, 9
    je .name_r9
    cmp rbx, 10
    je .name_r10
    cmp rbx, 11
    je .name_r11
    cmp rbx, 12
    je .name_r12
    cmp rbx, 13
    je .name_r13
    cmp rbx, 14
    je .name_r14
    cmp rbx, 15
    je .name_r15

.name_rax:
    mov byte [rcx], 'R'
    mov byte [rcx+1], 'A'
    mov byte [rcx+2], 'X'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_rbx:
    mov byte [rcx], 'R'
    mov byte [rcx+1], 'B'
    mov byte [rcx+2], 'X'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_rcx:
    mov byte [rcx], 'R'
    mov byte [rcx+1], 'C'
    mov byte [rcx+2], 'X'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_rdx:
    mov byte [rcx], 'R'
    mov byte [rcx+1], 'D'
    mov byte [rcx+2], 'X'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_rsi:
    mov byte [rcx], 'R'
    mov byte [rcx+1], 'S'
    mov byte [rcx+2], 'I'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_rdi:
    mov byte [rcx], 'R'
    mov byte [rcx+1], 'D'
    mov byte [rcx+2], 'I'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_rsp:
    mov byte [rcx], 'R'
    mov byte [rcx+1], 'S'
    mov byte [rcx+2], 'P'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_rbp:
    mov byte [rcx], 'R'
    mov byte [rcx+1], 'B'
    mov byte [rcx+2], 'P'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_r8:
    mov byte [rcx], 'R'
    mov byte [rcx+1], '8'
    mov byte [rcx+2], 0
    jmp .canon_done
.name_r9:
    mov byte [rcx], 'R'
    mov byte [rcx+1], '9'
    mov byte [rcx+2], 0
    jmp .canon_done
.name_r10:
    mov byte [rcx], 'R'
    mov byte [rcx+1], '1'
    mov byte [rcx+2], '0'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_r11:
    mov byte [rcx], 'R'
    mov byte [rcx+1], '1'
    mov byte [rcx+2], '1'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_r12:
    mov byte [rcx], 'R'
    mov byte [rcx+1], '1'
    mov byte [rcx+2], '2'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_r13:
    mov byte [rcx], 'R'
    mov byte [rcx+1], '1'
    mov byte [rcx+2], '3'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_r14:
    mov byte [rcx], 'R'
    mov byte [rcx+1], '1'
    mov byte [rcx+2], '4'
    mov byte [rcx+3], 0
    jmp .canon_done
.name_r15:
    mov byte [rcx], 'R'
    mov byte [rcx+1], '1'
    mov byte [rcx+2], '5'
    mov byte [rcx+3], 0
    jmp .canon_done

.canon_fail:
    ; return pointer to "UNK" (allocated)
    mov rax, SYS_KMALLOC_CODE
    mov rdi, 4
    syscall
    mov rcx, rax
    mov byte [rcx], 'U'
    mov byte [rcx+1], 'N'
    mov byte [rcx+2], 'K'
    mov byte [rcx+3], 0
    mov rax, rcx
    jmp .canon_ret

.canon_done:
    mov rax, rcx

.canon_ret:
    pop rcx
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
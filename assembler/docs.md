# BinBows 95 BEXE Assembler(BASm) Documentation

## Table of Contents
1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Basic Syntax](#basic-syntax)
4. [Beginner-Friendly Syntax](#beginner-friendly-syntax)
5. [Registers](#registers)
6. [Instructions](#instructions)
7. [Data Types and Sections](#data-types-and-sections)
8. [Labels and Jumps](#labels-and-jumps)
9. [Memory Addressing](#memory-addressing)
10. [Constants and Expressions](#constants-and-expressions)
11. [Macros](#macros)
12. [Preprocessor Directives](#preprocessor-directives)
13. [Syscalls](#syscalls)
14. [Complete Examples](#complete-examples)
15. [Error Handling](#error-handling)

---

## Introduction

The BEXE Assembler is a powerful tool for creating executable programs for the BinBows 95 operating system. It supports both traditional x86-style assembly syntax and beginner-friendly high-level syntax, making it accessible to both experienced programmers and newcomers.

### Features
- Full x86-style assembly syntax
- Beginner-friendly aliases and syntax
- Memory operands with multiple addressing modes
- Label support across .text, .data, and .bss sections
- Constants via EQU directive
- Parameterized macros
- File inclusion
- Expression evaluation
- Character literals and escape sequences

### File Format
The assembler produces BEXE (BinBows Executable) files with the following structure:
- **Header**: 10 bytes (magic number, code size, data size, BSS size)
- **Code Section**: Executable instructions
- **Data Section**: Initialized data
- **BSS Section**: Uninitialized data (not stored in file)

---

## Getting Started

### Installation
```bash
exec basm --help
```

### Basic Usage
```bash
# Simple assembly
exec basm output.bexe input.asm

# With verbose output
exec basm -v output.bexe input.asm

# Multiple input files
exec basm output.bexe file1.asm file2.asm file3.asm
```

### Your First Program
```nasm
.data
    msg: db "Hello, World!", 10, 0

.text
start:
    PRINT msg
    EXIT
```

Assemble it:
```bash
exec basm.py hello.bexe hello.asm
```

---

## Basic Syntax

### Comments
```nasm
; This is a single-line comment
mov rax, 5      ; Comments can appear after instructions
```

### Line Structure
```nasm
[label:] [instruction] [operands] [; comment]
```

### Case Insensitivity
Instructions and registers are case-insensitive:
```nasm
MOV rax, 5      ; Same as
mov RAX, 5      ; Same as
MoV RaX, 5      ; All valid
```

---

## Beginner-Friendly Syntax

The assembler provides high-level syntax for beginners:

### Assignment Operations
```nasm
; Traditional
mov rax, 42

; Beginner syntax
SET a TO 42
a = 42
LET a = 42
```

### Print Command
```nasm
; Traditional
mov rax, 1
mov rbx, msg
syscall

; Beginner syntax
PRINT msg
```

### Exit Command
```nasm
; Traditional
hlt

; Beginner syntax
EXIT
```

### Jumps
```nasm
; Traditional
jmp label
je label
jne label
jl label
jg label

; Beginner syntax
GOTO label
IF_EQUAL label
IF_NOT_EQUAL label
IF_LESS label
IF_GREATER label
```

### Simple Register Names
```nasm
; Use simple names instead of full register names
a = 10          ; Instead of: mov rax, 10
b = 20          ; Instead of: mov rbx, 20
c = 30          ; Instead of: mov rcx, 30
d = 40          ; Instead of: mov rdx, 40
```

---

## Registers

### 64-bit Registers
```nasm
rax, rbx, rcx, rdx    ; General purpose
rsp, rbp              ; Stack pointers
rsi, rdi              ; Source/Destination index
r8 - r15              ; Extended registers
```

### 32-bit Registers
```nasm
eax, ebx, ecx, edx
esp, ebp, esi, edi
```

### 16-bit Registers
```nasm
ax, bx, cx, dx
sp, bp, si, di
```

### 8-bit Registers
```nasm
al, bl, cl, dl        ; Low bytes
ah, bh, ch, dh        ; High bytes
```

### Beginner Aliases
```nasm
a  → rax
b  → rbx
c  → rcx
d  → rdx
```

---

## Instructions

### Data Movement
```nasm
mov dest, src       ; Move data (aliases: mv)
lea dest, [addr]    ; Load effective address
push src            ; Push onto stack
pop dest            ; Pop from stack
```

### Arithmetic
```nasm
add dest, src       ; Addition (alias: ad)
sub dest, src       ; Subtraction (alias: su)
mul dest, src       ; Multiplication (alias: mu)
div dest, src       ; Division (alias: di)
inc dest            ; Increment by 1
dec dest            ; Decrement by 1
neg dest            ; Negate (two's complement)
```

### Logical Operations
```nasm
and dest, src       ; Bitwise AND
or dest, src        ; Bitwise OR
xor dest, src       ; Bitwise XOR
not dest            ; Bitwise NOT
shl dest, count     ; Shift left
shr dest, count     ; Shift right
```

### Comparison
```nasm
cmp op1, op2        ; Compare two operands
                    ; Sets flags for conditional jumps
```

### Conditional Jumps
```nasm
je label            ; Jump if equal (alias: jz)
jne label           ; Jump if not equal (alias: jnz)
jl label            ; Jump if less (alias: jb)
jg label            ; Jump if greater (alias: ja)
jle label           ; Jump if less or equal (alias: jbe)
jge label           ; Jump if greater or equal (alias: jae)
```

### Unconditional Jump
```nasm
jmp label           ; Unconditional jump
```

### Subroutines
```nasm
call label          ; Call subroutine
ret                 ; Return from subroutine
```

### System
```nasm
syscall             ; System call (interrupt 0x80)
int number          ; Software interrupt
hlt                 ; Halt execution
nop                 ; No operation
```

---

## Data Types and Sections

### Sections
```nasm
.text               ; Code section (executable)
.data               ; Initialized data section
.bss                ; Uninitialized data section
```

### Data Directives

#### Define Data
```nasm
db value            ; Define byte (1 byte)
dw value            ; Define word (2 bytes)
dd value            ; Define double word (4 bytes)
dq value            ; Define quad word (8 bytes)

; Aliases
byte value          ; Same as db
word value          ; Same as dw
dword value         ; Same as dd
qword value         ; Same as dq
```

#### Reserve Space (BSS only)
```nasm
resb count          ; Reserve bytes
resw count          ; Reserve words
resd count          ; Reserve double words
resq count          ; Reserve quad words
```

### Data Examples
```nasm
.data
    ; Strings
    msg: db "Hello", 10, 0
    
    ; Numbers
    byte_val: db 255
    word_val: dw 65535
    dword_val: dd 4294967295
    qword_val: dq 18446744073709551615
    
    ; Arrays
    numbers: dd 1, 2, 3, 4, 5
    buffer: db 100 dup(0)  ; 100 zeros

.bss
    ; Uninitialized storage
    temp: resb 1
    array: resd 100
```

---

## Labels and Jumps

### Defining Labels
```nasm
.text
start:              ; Label in code section
    mov rax, 1

loop_start:         ; Another label
    inc rax
    jmp loop_start

.data
message: db "Text" ; Label in data section

.bss
buffer: resb 256   ; Label in BSS section
```

### Using Labels
```nasm
; Jump to label
jmp start

; Conditional jump to label
cmp rax, 10
je equal_label

; Call subroutine
call my_function

; Load address of label
mov rbx, message

; Access data at label
mov rax, [buffer]
```

---

## Memory Addressing

### Direct Memory Access
```nasm
mov rax, [address]      ; Load from memory
mov [address], rax      ; Store to memory
```

### Register Indirect
```nasm
mov rax, [rbx]          ; Load from address in rbx
mov [rbx], rax          ; Store to address in rbx
```

### Register + Displacement
```nasm
mov rax, [rbx+8]        ; Load from rbx+8
mov [rbx+16], rax       ; Store to rbx+16
mov rax, [rbx-4]        ; Negative displacement
```

### Register + Register
```nasm
mov rax, [rbx+rcx]      ; Base + index
```

### Register + Register + Displacement
```nasm
mov rax, [rbx+rcx+8]    ; Base + index + displacement
```

### Size Prefixes
```nasm
mov al, byte [rbx]      ; Load byte
mov ax, word [rbx]      ; Load word
mov eax, dword [rbx]    ; Load dword
mov rax, qword [rbx]    ; Load qword

; Alternative syntax
mov al, byte ptr [rbx]
mov ax, word ptr [rbx]
```

### Label-based Addressing
```nasm
.data
    array: dd 10, 20, 30, 40

.text
    mov rax, [array]        ; First element
    mov rbx, [array+4]      ; Second element (4-byte offset)
    mov rcx, [array+8]      ; Third element
```

---

## Constants and Expressions

### EQU Directive
```nasm
; Define constants
MAX_SIZE equ 100
BUFFER_LEN equ 256
TRUE equ 1
FALSE equ 0

; Use constants
mov rax, MAX_SIZE
cmp rbx, BUFFER_LEN
```

### Expression Evaluation
```nasm
; Arithmetic in constants
SIZE equ 10
DOUBLE_SIZE equ SIZE * 2
TOTAL equ SIZE + DOUBLE_SIZE

; Use in instructions
mov rax, SIZE * 4
add rbx, (MAX_SIZE - 10)
```

### Numeric Formats
```nasm
mov rax, 42             ; Decimal
mov rax, 0x2A           ; Hexadecimal
mov rax, 0b101010       ; Binary
mov rax, 0o52           ; Octal
```

### Character Literals
```nasm
mov al, 'A'             ; Single character
mov al, 65              ; Same as 'A'

; String in data section
msg: db "Hello", 0
```

### Escape Sequences
```nasm
.data
    newline: db "Line 1", 10, "Line 2", 0  ; \n = 10
    tab: db "Col1", 9, "Col2", 0            ; \t = 9
    null: db "String", 0                    ; \0 = 0
    
    ; Using escape in strings
    escaped: db "Line1\nLine2\tTabbed\0"
```

---

## Macros

### Defining Macros
```nasm
%macro macro_name param_count
    ; Macro body
    ; Use %1, %2, %3... for parameters
%endmacro
```

### Simple Macro
```nasm
%macro PRINT_SYSCALL 1
    mov rax, 1
    mov rbx, %1
    syscall
%endmacro

; Usage
.data
    msg: db "Hello", 0

.text
    PRINT_SYSCALL msg
```

### Multi-Parameter Macro
```nasm
%macro ADD_THREE 3
    mov rax, %1
    add rax, %2
    add rax, %3
%endmacro

; Usage
ADD_THREE 10, 20, 30    ; rax = 60
```

### Nested Macro Example
```nasm
%macro SAVE_REGS 0
    push rax
    push rbx
    push rcx
%endmacro

%macro RESTORE_REGS 0
    pop rcx
    pop rbx
    pop rax
%endmacro

%macro SAFE_CALL 1
    SAVE_REGS
    call %1
    RESTORE_REGS
%endmacro

; Usage
SAFE_CALL my_function
```

---

## Preprocessor Directives

### File Inclusion
```nasm
%include "common.asm"
%include "macros.asm"
```

Include file example (`common.asm`):
```nasm
; common.asm - Common definitions
SCREEN_WIDTH equ 640
SCREEN_HEIGHT equ 480

%macro CLEAR_REG 1
    xor %1, %1
%endmacro
```

Main file:
```nasm
%include "common.asm"

.text
start:
    mov rax, SCREEN_WIDTH
    CLEAR_REG rbx
```

### Include Path Resolution
The assembler searches for includes:
1. Current directory
2. Relative to the file containing the `%include`

### Circular Include Prevention
The assembler automatically detects and prevents circular includes.

---

## Syscalls

### System Call Numbers
```nasm
SYS_RESERVED    = 0
SYS_FB_PRINT    = 1
SYS_READ_FILE   = 2
SYS_WRITE_FILE  = 3
SYS_OPEN_FILE   = 4
SYS_CLOSE_FILE  = 5
SYS_KMALLOC     = 6
SYS_KFREE       = 7
SYS_MEMCPY      = 8
SYS_MEMCMP      = 9
SYS_MEMSET      = 10
```

### Making System Calls

#### Method 1: Direct Syscall
```nasm
mov rax, 1              ; SYS_FB_PRINT
mov rbx, msg            ; Argument
syscall                 ; Trigger interrupt 0x80
```

#### Method 2: Using INT
```nasm
mov rax, 1
mov rbx, msg
int 0x80
```

#### Method 3: Beginner Syntax
```nasm
PRINT msg
```

### Print String Example
```nasm
.data
    message: db "Hello, World!", 10, 0

.text
start:
    mov rax, SYS_FB_PRINT
    mov rbx, message
    syscall
    hlt
```

### File Operations Example
```nasm
.data
    filename: db "test.txt", 0
    buffer: resb 256

.text
start:
    ; Open file
    mov rax, SYS_OPEN_FILE
    mov rbx, filename
    syscall
    
    ; Read file (file handle in rax)
    mov rbx, rax            ; File handle
    mov rax, SYS_READ_FILE
    mov rcx, buffer         ; Buffer
    mov rdx, 256            ; Size
    syscall
    
    ; Close file
    mov rax, SYS_CLOSE_FILE
    syscall
    
    hlt
```

---

## Complete Examples

### Example 1: Hello World
```nasm
.data
    msg: db "Hello, World!", 10, 0

.text
start:
    PRINT msg
    EXIT
```

### Example 2: Counter Loop
```nasm
.data
    done: db "Counting complete!", 10, 0

.text
start:
    a = 0               ; counter
    b = 10              ; limit

count_loop:
    inc a
    cmp a, b
    IF_LESS count_loop
    
    PRINT done
    EXIT
```

### Example 3: Simple Calculator
```nasm
.data
    num1: dd 42
    num2: dd 58
    result: dd 0
    msg: db "Calculation done!", 10, 0

.text
start:
    ; Load numbers
    mov rax, [num1]
    mov rbx, [num2]
    
    ; Calculate
    add rax, rbx
    
    ; Store result
    mov [result], rax
    
    ; Print message
    PRINT msg
    EXIT
```

### Example 4: Conditional Logic
```nasm
.data
    pass: db "You passed!", 10, 0
    fail: db "You failed!", 10, 0

.text
start:
    SET a TO 85         ; grade
    
    cmp a, 60
    IF_GREATER_OR_EQUAL passed
    
    PRINT fail
    GOTO done
    
passed:
    PRINT pass
    
done:
    EXIT
```

### Example 5: Function Call
```nasm
.data
    msg1: db "Before call", 10, 0
    msg2: db "Inside function", 10, 0
    msg3: db "After return", 10, 0

.text
start:
    PRINT msg1
    call my_function
    PRINT msg3
    EXIT

my_function:
    PRINT msg2
    ret
```

### Example 6: Array Processing
```nasm
.data
    numbers: dd 10, 20, 30, 40, 50
    sum: dd 0
    msg: db "Sum calculated!", 10, 0

.text
start:
    mov rax, 0              ; sum accumulator
    mov rbx, numbers        ; array pointer
    mov rcx, 5              ; count
    
sum_loop:
    add rax, [rbx]          ; add current element
    add rbx, 4              ; move to next (4 bytes per dd)
    dec rcx
    jnz sum_loop
    
    mov [sum], rax
    PRINT msg
    EXIT
```

### Example 7: String Length
```nasm
.data
    text: db "Hello, World!", 0
    length: dd 0

.text
start:
    mov rbx, text           ; pointer to string
    mov rcx, 0              ; length counter
    
strlen_loop:
    mov al, [rbx]           ; load byte
    cmp al, 0               ; check for null
    je strlen_done
    inc rcx                 ; increment length
    inc rbx                 ; next character
    jmp strlen_loop
    
strlen_done:
    mov [length], rcx
    hlt
```

### Example 8: Macro Usage
```nasm
; Define macros
%macro PRINT_MSG 1
    mov rax, 1
    mov rbx, %1
    syscall
%endmacro

%macro SAVE_STATE 0
    push rax
    push rbx
    push rcx
%endmacro

%macro RESTORE_STATE 0
    pop rcx
    pop rbx
    pop rax
%endmacro

.data
    msg1: db "Message 1", 10, 0
    msg2: db "Message 2", 10, 0

.text
start:
    SAVE_STATE
    PRINT_MSG msg1
    PRINT_MSG msg2
    RESTORE_STATE
    hlt
```

### Example 9: Temperature Checker
```nasm
.data
    cold: db "Too cold!", 10, 0
    hot: db "Too hot!", 10, 0
    perfect: db "Perfect temperature!", 10, 0

.text
start:
    a = 72                  ; temperature
    
    ; Check if too cold (< 60)
    cmp a, 60
    IF_LESS too_cold_label
    
    ; Check if too hot (> 80)
    cmp a, 80
    IF_GREATER too_hot_label
    
    ; Perfect range
    PRINT perfect
    GOTO done
    
too_cold_label:
    PRINT cold
    GOTO done
    
too_hot_label:
    PRINT hot
    
done:
    EXIT
```

### Example 10: Multiplication Table
```nasm
.data
    result: dd 0

.text
start:
    SET a TO 5              ; multiplicand
    SET b TO 1              ; multiplier
    SET c TO 10             ; limit
    
multiply_loop:
    ; Calculate a * b
    mov rax, a
    mul rax, b
    mov [result], rax
    
    ; Next
    inc b
    cmp b, c
    IF_LESS_OR_EQUAL multiply_loop
    
    EXIT
```

---

## Error Handling

### Common Errors

#### Undefined Label
```nasm
jmp undefined_label     ; ERROR: Undefined label
```
**Solution**: Make sure the label is defined.

#### Invalid Operand
```nasm
mov 5, rax              ; ERROR: Cannot move to immediate
```
**Solution**: Check operand order (destination, source).

#### Memory Operand Missing Brackets
```nasm
mov rax, rbx+8          ; ERROR: Missing brackets for memory
```
**Solution**: Use `[rbx+8]` for memory access.

#### Undefined Constant
```nasm
mov rax, UNDEFINED      ; ERROR: Undefined constant
```
**Solution**: Define with `UNDEFINED equ value`.

#### Section Mismatch
```nasm
.text
    my_data: db "Hello"  ; ERROR: Data in text section
```
**Solution**: Use appropriate section (.data for data).

#### Circular Include
```nasm
; file1.asm
%include "file2.asm"

; file2.asm
%include "file1.asm"    ; ERROR: Circular include
```
**Solution**: Reorganize includes to avoid circular dependencies.

#### Invalid Macro Parameters
```nasm
%macro TEST 2
    mov %1, %2
%endmacro

TEST rax                ; ERROR: Expected 2 parameters, got 1
```
**Solution**: Provide correct number of parameters.

### Debugging Tips

#### Use Verbose Mode
```bash
exec basm.py -v output.bexe input.asm
```

This shows:
- Label definitions and addresses
- Instruction encoding
- Memory layout
- Hex dumps of code and data

#### Check Generated Output
```bash
exec basm.py -v hello.bexe hello.asm
```

Output includes:
```
Text labels:
  start                = 0x0000

Data labels:
  msg                  = 0x000a (data+0)

Code section hex dump:
  0000: 01 00 01 0a 00 00 00 50 40
```

#### Common Debugging Steps
1. Check label spelling and definition
2. Verify operand types and order
3. Ensure proper section usage
4. Check bracket usage for memory access
5. Verify macro parameter counts

---

## Quick Reference

### Instruction Summary
| Category | Instructions |
|----------|-------------|
| Data Movement | mov, lea, push, pop |
| Arithmetic | add, sub, mul, div, inc, dec, neg |
| Logical | and, or, xor, not, shl, shr |
| Comparison | cmp |
| Jumps | jmp, je, jne, jl, jg, jle, jge |
| Functions | call, ret |
| System | syscall, int, hlt, nop |

### Beginner Aliases
| Beginner | Traditional |
|----------|-------------|
| SET x TO y | mov x, y |
| x = y | mov x, y |
| PRINT msg | mov rax, 1; mov rbx, msg; syscall |
| EXIT | hlt |
| GOTO label | jmp label |
| IF_EQUAL | je |
| IF_NOT_EQUAL | jne |
| IF_LESS | jl |
| IF_GREATER | jg |
| a, b, c, d | rax, rbx, rcx, rdx |

### Data Types
| Directive | Size | Bytes |
|-----------|------|-------|
| db / byte | byte | 1 |
| dw / word | word | 2 |
| dd / dword | dword | 4 |
| dq / qword | qword | 8 |

### Sections
| Section | Purpose |
|---------|---------|
| .text | Executable code |
| .data | Initialized data |
| .bss | Uninitialized data |

---

## Appendix: Full Example Program

```nasm
; Complete example: Menu system with multiple functions

; Constants
OPTION_A equ 1
OPTION_B equ 2
OPTION_C equ 3

; Macros
%macro PRINT_LINE 1
    mov rax, 1
    mov rbx, %1
    syscall
%endmacro

.data
    ; Menu strings
    menu: db "=== MENU ===", 10, 0
    opt_a: db "Option A selected", 10, 0
    opt_b: db "Option B selected", 10, 0
    opt_c: db "Option C selected", 10, 0
    invalid: db "Invalid option", 10, 0
    goodbye: db "Goodbye!", 10, 0
    
    ; Data storage
    selection: dd 0
    counter: dd 0

.bss
    buffer: resb 256

.text
start:
    ; Initialize
    mov [counter], dword 0
    
    ; Show menu
    PRINT_LINE menu
    
    ; Simulate selection
    SET a TO OPTION_A
    mov [selection], a
    
    ; Process selection
    mov rax, [selection]
    cmp rax, OPTION_A
    IF_EQUAL option_a_handler
    
    cmp rax, OPTION_B
    IF_EQUAL option_b_handler
    
    cmp rax, OPTION_C
    IF_EQUAL option_c_handler
    
    ; Invalid
    PRINT_LINE invalid
    GOTO finish

option_a_handler:
    PRINT_LINE opt_a
    call increment_counter
    GOTO finish

option_b_handler:
    PRINT_LINE opt_b
    call increment_counter
    GOTO finish

option_c_handler:
    PRINT_LINE opt_c
    call increment_counter
    GOTO finish

finish:
    PRINT_LINE goodbye
    EXIT

; Helper function
increment_counter:
    mov rax, [counter]
    inc rax
    mov [counter], rax
    ret
```

---

## Resources

### Command Line Help
```bash
exec basm --help
```

### Version Information
```
BinBows 95 BEXE Assembler v1.1
```

### Contact and Support
For issues and questions, refer to the BinBows 95 documentation.

---

**End of Documentation**
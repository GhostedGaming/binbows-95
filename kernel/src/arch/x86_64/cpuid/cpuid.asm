section .data
vendor_string: times 12 db 0

section .text
global get_vendor

get_vendor:
    mov eax, 0
    cpuid
    mov [vendor_string + 0], ebx
    mov [vendor_string + 4], edx
    mov [vendor_string + 8], ecx
    lea rax, [vendor_string]
    ret
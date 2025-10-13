#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <panic.h>
#include <serial.h>
#include <framebuffer.h>

#define panic(fmt, ...) panic_impl(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

void panic_impl(const char *file, int line, const char *func, const char *fmt, ...) {
    asm volatile ("cli");
    
    clear_screen();

    fb_printf("Kernel Panic!\n", rgb_to_color(255, 0, 0));
    fb_printf("Location: %s:%d in %s()\n", file, line, func);

    serial_printf("\n*** KERNEL PANIC ***\n");
    serial_printf("Location: %s:%d in %s()\n", file, line, func);
    serial_printf("Reason: ");
    
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    kvsnprintf(buffer, sizeof(buffer), fmt, args);
    serial_printf("%s\n", buffer);
    va_end(args);
        
    print_registers();
    print_stack_trace();
    
    serial_printf("\nSystem halted.\n");
    
    while (1) {
        asm volatile("hlt");
    }
}

void print_registers(void) {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rflags, cr0, cr2, cr3;
    
    asm volatile("mov %%rax, %0" : "=m"(rax));
    asm volatile("mov %%rbx, %0" : "=m"(rbx));
    asm volatile("mov %%rcx, %0" : "=m"(rcx));
    asm volatile("mov %%rdx, %0" : "=m"(rdx));
    asm volatile("mov %%rsi, %0" : "=m"(rsi));
    asm volatile("mov %%rdi, %0" : "=m"(rdi));
    asm volatile("mov %%rbp, %0" : "=m"(rbp));
    asm volatile("mov %%rsp, %0" : "=m"(rsp));
    asm volatile("mov %%r8, %0" : "=m"(r8));
    asm volatile("mov %%r9, %0" : "=m"(r9));
    asm volatile("mov %%r10, %0" : "=m"(r10));
    asm volatile("mov %%r11, %0" : "=m"(r11));
    asm volatile("mov %%r12, %0" : "=m"(r12));
    asm volatile("mov %%r13, %0" : "=m"(r13));
    asm volatile("mov %%r14, %0" : "=m"(r14));
    asm volatile("mov %%r15, %0" : "=m"(r15));
    asm volatile("pushfq\n popq %0" : "=m"(rflags));
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    
    serial_printf("Register dump:\n");
    serial_printf("RAX: 0x%016lx RBX: 0x%016lx RCX: 0x%016lx RDX: 0x%016lx\n", rax, rbx, rcx, rdx);
    serial_printf("RSI: 0x%016lx RDI: 0x%016lx RBP: 0x%016lx RSP: 0x%016lx\n", rsi, rdi, rbp, rsp);
    serial_printf("R8: 0x%016lx R9: 0x%016lx R10: 0x%016lx R11: 0x%016lx\n", r8, r9, r10, r11);
    serial_printf("R12: 0x%016lx R13: 0x%016lx R14: 0x%016lx R15: 0x%016lx\n", r12, r13, r14, r15);
    serial_printf("RFLAGS: 0x%016lx\n", rflags);
    serial_printf("CR0: 0x%016lx CR2: 0x%016lx CR3: 0x%016lx\n", cr0, cr2, cr3);
}

void print_stack_trace(void) {
    serial_printf("Stack trace:\n");
    
    uint64_t *rbp;
    asm volatile("mov %%rbp, %0" : "=r"(rbp));
    
    int frame_count = 0;
    const int max_frames = 16;
    
    while (rbp && frame_count < max_frames) {
        if ((uintptr_t)rbp < 0x1000 || (uintptr_t)rbp > 0x7fffffffffff) {
            serial_printf(" [%d] Invalid frame pointer: %p\n", frame_count, rbp);
            break;
        }
        
        uint64_t return_addr = *(rbp + 1);
        serial_printf(" [%d] RBP: %p Return: %p\n", frame_count, rbp, (void*)return_addr);
        
        uint64_t *prev_rbp = (uint64_t*)*rbp;
        if (prev_rbp <= rbp) {
            serial_printf(" [%d] Stack frame chain broken\n", frame_count + 1);
            break;
        }
        
        rbp = prev_rbp;
        frame_count++;
    }
    
    if (frame_count == 0) {
        serial_printf(" No valid stack frames found\n");
    }
}
#include <enable_sse.h>
#include <stdint.h>

void enable_sse() {
    uint64_t cr0, cr4;

    asm volatile ("mov %%cr0, %0" : "=r"(cr0));

    cr0 &= ~(1 << 2);
    cr0 |= (1 << 1);

    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    asm volatile("mov %%cr4, %0" : "=r"(cr4));

    cr4 |= (1 << 9);
    cr4 |= (1 << 10);
    
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
    
    asm volatile("fninit");
}
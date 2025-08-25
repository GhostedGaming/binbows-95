#include <stdint.h>
#include <stddef.h>
#include <serial.h>
#include <gdt.h>
#include <mem.h>

#define GDT_ENTRIES 7

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gdt_pointer;
static struct tss kernel_tss;

// Set a GDT entry
static void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt[index].base_low = (base & 0xFFFF);
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    
    gdt[index].limit_low = (limit & 0xFFFF);
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= granularity & 0xF0;
    gdt[index].access = access;
}

// Set TSS entry (takes 2 GDT entries in x86_64)
static void gdt_set_tss(int index, uint64_t base, uint32_t limit) {
    gdt_set_entry(index, base & 0xFFFFFFFF, limit, 0x89, 0x00);
    
    // High part of TSS descriptor
    gdt[index + 1].limit_low = (base >> 32) & 0xFFFF;
    gdt[index + 1].base_low = (base >> 48) & 0xFFFF;
    gdt[index + 1].base_middle = 0;
    gdt[index + 1].access = 0;
    gdt[index + 1].granularity = 0;
    gdt[index + 1].base_high = 0;
}

void gdt_init(void) {
    gdt_pointer.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdt_pointer.base = (uint64_t)&gdt;
    
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Kernel code segment (64-bit)
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xAF);
    
    // Kernel data segment
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    
    // User code segment (64-bit)
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xAF);
    
    // User data segment
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
    
    // Initialize TSS - clear it first
    memset(&kernel_tss, 0, sizeof(struct tss));
    kernel_tss.rsp0 = 0; // This is OK for now, will be set later
    kernel_tss.iopb_offset = sizeof(struct tss);
    
    // TSS descriptor at index 5 (takes entries 5 and 6)
    gdt_set_tss(5, (uint64_t)&kernel_tss, sizeof(struct tss) - 1);
}

void gdt_load(void) {
    write_serial("GDT: gdt_flush\n");
    
    // Inline assembly to load GDT
    asm volatile (
        "lgdt %0\n\t"
        "mov %w1, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq %2\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        :
        : "m" (gdt_pointer), "r" ((uint16_t)0x10), "r" ((uint64_t)0x08)
        : "rax", "memory"
    );
    
    write_serial("Complete\n");

    write_serial("TSS: loading TSS\n");
    
    // Add a small delay and ensure TSS is properly set up
    for(volatile int i = 0; i < 1000; i++);
    
    // Load TSS with correct selector (index 5 = 0x28)
    asm volatile(
        "ltr %0\n\t"
        : 
        : "r"((uint16_t)0x28)
        : "memory"
    );
    
    write_serial("Complete\n");
}
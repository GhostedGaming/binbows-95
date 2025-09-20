#include <stdint.h>
#include <stddef.h>
#include <serial.h>
#include <gdt.h>
#include <util.h>

#define GDT_ENTRIES 7

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gdt_pointer;
static struct tss kernel_tss;

static void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt[index].base_low = (base & 0xFFFF);
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    
    gdt[index].limit_low = (limit & 0xFFFF);
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= granularity & 0xF0;
    gdt[index].access = access;
}

void gdt_load(void) {
    // Load the GDT
    asm volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"      // 0x10 = kernel data segment selector
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"          // 0x08 = kernel code segment selector
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        :
        : "m" (gdt_pointer)
        : "rax", "memory"
    );

    write_serial("Complete\n");

    write_serial("TSS: loading TSS\n");

    // Load TSS (selector 0x28, index 5)
    asm volatile (
        "ltr %0\n\t"
        :
        : "r"((uint16_t)0x28)
        : "memory"
    );

    write_serial("Complete\n");
}

void gdt_init(void) {
    gdt_pointer.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdt_pointer.base = (uint64_t)&gdt;
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Kernel code segment (64-bit)
    // Base = 0
    // Limit = 0xFFFFF
    // Access Byte = 0x9A
    // Flags = 0xA
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xAF);

    // Kernel mode data segment
    // Base = 0
    // Limit = 0xFFFFF
    // Access Byte = 0x92
    // Flags = 0xC
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);

    // User mode code segment
    // Base = 0
    // Limit = 0xFFFFF
    // Access Byte = 0xFA
    // Flags = 0xA
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xAF);

    // User mode data segment
    // Base = 0
    // Limit = 0xFFFFF
    // Access Byte = 0xF2
    // Flags = 0xC
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);

    // Task State Segment (TSS)
    // Base = &TSS
    // Limit = sizeof(TSS)-1
    // Access Byte = 0x89
    // Flags = 0x0
    memset(&kernel_tss, 0, sizeof(struct tss));
    kernel_tss.rsp0 = 0;
    kernel_tss.iopb_offset = sizeof(struct tss);
    gdt_set_entry(5, (uint64_t)&kernel_tss & 0xFFFFFFFF, sizeof(struct tss)-1, 0x89, 0x00);
}
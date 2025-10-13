#include <stdint.h>
#include <stddef.h>
#include <serial.h>
#include <gdt.h>
#include <util.h>

#define GDT_ENTRIES 7

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gdt_pointer;
static struct tss kernel_tss;
static uint8_t kernel_stack[4096] __attribute__((aligned(16)));

static void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt[index].base_low = (base & 0xFFFF);
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    gdt[index].limit_low = (limit & 0xFFFF);
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= granularity & 0xF0;
    gdt[index].access = access;
}

static void gdt_set_tss(int index, uint64_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    uint64_t desc_low = 0;
    desc_low |= (uint64_t)(limit & 0xFFFF);
    desc_low |= (uint64_t)(base & 0xFFFF) << 16;
    desc_low |= (uint64_t)((base >> 16) & 0xFF) << 32;
    desc_low |= (uint64_t)access << 40;
    desc_low |= (uint64_t)((limit >> 16) & 0x0F) << 48;
    desc_low |= (uint64_t)(granularity & 0xF0) << 48;
    desc_low |= (uint64_t)((base >> 24) & 0xFF) << 56;

    uint64_t desc_high = 0;
    desc_high |= (uint64_t)(base >> 32) & 0xFFFFFFFFULL;

    {
        uint8_t *dst = (uint8_t *)gdt + (index * sizeof(uint64_t));
        uint64_t vals[2] = { desc_low, desc_high };
        for (int k = 0; k < 2; k++) {
            uint64_t v = vals[k];
            for (int i = 0; i < 8; i++) {
                dst[k * 8 + i] = (uint8_t)(v & 0xFF);
                v >>= 8;
            }
        }
    }
}

void gdt_init(void) {
    gdt_pointer.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdt_pointer.base = (uint64_t)&gdt;
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xAF);
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xAF);
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
    memset(&kernel_tss, 0, sizeof(struct tss));
    kernel_tss.rsp0 = (uint64_t)&kernel_stack[4096];
    kernel_tss.iopb_offset = sizeof(struct tss);
    gdt_set_tss(5, (uint64_t)&kernel_tss, sizeof(struct tss) - 1, 0x89, 0x00);
}

void gdt_load(void) {
    asm volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        :
        : "m" (gdt_pointer)
        : "rax", "memory"
    );
    asm volatile ("ltr %0" :: "r"((uint16_t)0x28) : "memory");
}

void test_user_function(void) {
    write_serial("User mode running\n");
    asm volatile (
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"
        "leaq return_to_kernel(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "return_to_kernel:\n\t"
        :
        :
        : "rax"
    );
    write_serial("Back in kernel\n");
}

void jump_to_ring3(void) {
    static uint64_t user_stack_space[512] __attribute__((aligned(16)));
    uint64_t user_rsp = (uint64_t)&user_stack_space[512];
    asm volatile (
        "cli\n\t"
        "pushq $0x23\n\t"
        "pushq %0\n\t"
        "pushfq\n\t"
        "pushq $0x1B\n\t"
        "pushq %1\n\t"
        "iretq\n\t"
        :
        : "r"(user_rsp), "r"(test_user_function)
        : "memory"
    );
}

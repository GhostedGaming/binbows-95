#include <stddef.h>
#include <stdint.h>
#include <interrupts.h>
#include <pic.h>
#include <serial.h>
#include <ps2_keyboard.h>
#include <timer.h>
#include <io.h>
#include <scheduler.h>

// This file hold all the interrupts and exceptions needed

#define IA32_STAR      0xC0000081
#define IA32_LSTAR     0xC0000082
#define IA32_FMASK     0xC0000084
#define IA32_KERNEL_GS_BASE 0xC0000102

// IDT definition
__attribute__((aligned(0x10))) static idt_entry_t idt[256];
static idtr_t idtr;

extern void isr0(void), isr1(void), isr2(void), isr3(void), isr4(void), isr5(void);
extern void isr6(void), isr7(void), isr8(void), isr9(void), isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void), isr16(void), isr17(void);
extern void isr18(void), isr19(void), isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void), isr28(void), isr29(void);
extern void isr30(void), isr31(void);

extern void irq0(void); // Timer
extern void irq1(void); // Keyboard

static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static void idt_add_entry(uint8_t vector, void (*isr)(void), uint8_t flags) {
    uint64_t addr = (uint64_t)(uintptr_t)isr;
    idt[vector].isr_low = addr & 0xFFFF;
    idt[vector].kernel_cs = 0x08;
    idt[vector].ist = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

// Initialize exceptions (ISRs 0-31)
void install_exceptions(void) {
    void (*exceptions[])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        idt_add_entry(i, exceptions[i], 0x8E); // Present, DPL=0, interrupt gate
    }

    write_serial("IDT: CPU exception handlers installed\n");
}

void install_irq_common(uint8_t irq_vector, void (*handler)(void)) {
    idt_add_entry(irq_vector, handler, 0x8E);
}

void init_timer_irq(void) {
    install_irq_common(32, irq0);
    write_serial("IDT: Timer interrupt (IRQ0) installed\n");
}

void init_keyboard_irq(void) {
    install_irq_common(33, irq1);
    write_serial("IDT: Keyboard interrupt (IRQ1) installed\n");
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt[i] = (idt_entry_t){0};
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    write_serial("IDT: Initialized and cleared\n");
}

void idt_load(void) {
    __asm__ volatile ("lidt %0" : : "m"(idtr));
    write_serial("IDT: Loaded into CPU\n");
}

void isr_handler(uint64_t interrupt_number) {
    serial_printf("=== CPU Exception %lu ===\n", interrupt_number);

    switch(interrupt_number) {
        case 0: 
            serial_printf("Division by zero\n"); 
            break;
        case 6: 
            serial_printf("Invalid opcode\n");
            break;
        case 8: 
            serial_printf("Double fault\n"); 
            break;
        case 13: 
            serial_printf("General protection fault\n");
            break;
        case 14: {
            uint64_t fault_addr;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
            
            serial_printf("Page fault at address: 0x%lx\n", fault_addr);
            
            if (fault_addr == 0x100000) {
                serial_printf("*** Page fault at binary execution address 0x100000 ***\n");
                serial_printf("The memory is not mapped or not executable!\n");
            }
            break;
        }
        default: 
            serial_printf("Unhandled exception\n"); 
            break;
    }

    serial_printf("=== System Halted ===\n");
    __asm__ volatile ("cli\nhlt");
}

void irq_handler(uint64_t irq_number) {
    switch(irq_number) {
        case 32: // IRQ0 - Timer
            on_irq0();
            // change_process();
            serial_printf(".");
            break;
        case 33: // IRQ1 - Keyboard
            keyboard_handler(NULL);
            break;
        default:
            write_serial("Unhandled IRQ\n");
            break;
    }

    // Send EOI to PICs
    if (irq_number >= 40) {
        outb(0xA0, 0x20); // Slave PIC
    }
    outb(0x20, 0x20); // Master PIC
}
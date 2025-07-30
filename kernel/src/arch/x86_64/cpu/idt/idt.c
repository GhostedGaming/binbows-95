#include <stddef.h>
#include <stdint.h>
#include <idt.h>
#include <pic.h>
#include <serial.h>
// #include "../../ps2_keyboard/keyboard.h"
#include <timer.h>
#include <io.h>

// IDT definition
__attribute__((aligned(0x10))) static idt_entry_t idt[256];
static idtr_t idtr;

// ISR and IRQ handler declarations (from isr.asm or irq.asm)
extern void isr0(void), isr1(void), isr2(void), isr3(void), isr4(void), isr5(void);
extern void isr6(void), isr7(void), isr8(void), isr9(void), isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void), isr16(void), isr17(void);
extern void isr18(void), isr19(void), isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void), isr28(void), isr29(void);
extern void isr30(void), isr31(void);

extern void irq0(void); // Timer
extern void irq1(void); // Keyboard

static void idt_add_entry(uint8_t vector, void* isr, uint8_t flags) {
    uint64_t addr = (uint64_t)isr;
    idt[vector].isr_low = addr & 0xFFFF;
    idt[vector].kernel_cs = 0x08;
    idt[vector].ist = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

void install_exceptions(void) {
    void (*exceptions[])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        idt_add_entry(i, exceptions[i], 0x8E);
    }

    write_serial("IDT: CPU exception handlers installed\n");
}

void install_irq_common(uint8_t irq_vector, void* handler) {
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
    write_serial("CPU Exception!\n");

    switch(interrupt_number) {
        case 0: write_serial("Division by zero\n"); break;
        case 6: write_serial("Invalid opcode\n"); break;
        case 8: write_serial("Double fault\n"); break;
        case 13: write_serial("General protection fault\n"); break;
        case 14: write_serial("Page fault\n"); break;
        default: write_serial("Unhandled exception\n"); break;
    }

    __asm__ volatile ("cli\nhlt");
}

void irq_handler(uint64_t irq_number) {
    switch(irq_number) {
        case 32: // IRQ0 - Timer
            // write_serial("Tick");
            // ^ Only uncomment if you need to debug ^
            on_irq0();
            break;
        case 33:
            write_serial("Keyboard IRQ received\n");
            // ^ Only uncomment if you need to debug ^
            //keyboard_handler(NULL);
            break;
        default:
            write_serial("Unhandled IRQ\n");
            break;
    }

    // Send EOI to PIC
    if (irq_number >= 40) {
        outb(0xA0, 0x20); // Send EOI to slave PIC
    }
    outb(0x20, 0x20); // Send EOI to master PIC
}

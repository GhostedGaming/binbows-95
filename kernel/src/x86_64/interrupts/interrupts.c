#include <interrupts.h>
#include <pic.h>
#include <serial.h>
#include <ps2_keyboard.h>
#include <timer.h>
#include <io.h>
#include <scheduler.h>

// IDT structure
__attribute__((aligned(0x10))) static idt_entry_t idt[256];
static idtr_t idtr;

// External assembly handlers
extern void isr0(void), isr1(void), isr2(void), isr3(void);
extern void isr4(void), isr5(void), isr6(void), isr7(void);
extern void isr8(void), isr9(void), isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void);
extern void isr16(void), isr17(void), isr18(void), isr19(void);
extern void isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void);
extern void isr28(void), isr29(void), isr30(void), isr31(void);
extern void irq0(void);   // Timer
extern void irq1(void);   // Keyboard
extern void isr128(void); // Syscall

// Set an IDT entry
static void idt_set_entry(uint8_t vector, void (*handler)(void), uint8_t flags) {
    uint64_t addr = (uint64_t)handler;
    
    idt[vector].isr_low    = addr & 0xFFFF;
    idt[vector].kernel_cs  = 0x08;
    idt[vector].ist        = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid    = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high   = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved   = 0;
}

// Initialize IDT
void idt_init(void) {
    // Clear IDT
    for (int i = 0; i < 256; i++) {
        idt[i] = (idt_entry_t){0};
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    write_serial("IDT: Initialized\n");
}

// Load IDT into CPU
void idt_load(void) {
    __asm__ volatile ("lidt %0" : : "m"(idtr));
    write_serial("IDT: Loaded\n");
}

// Install all exception handlers (0-31)
void install_exceptions(void) {
    void (*exceptions[])(void) = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        idt_set_entry(i, exceptions[i], 0x8E);
    }

    write_serial("IDT: Exceptions installed\n");
}

// Install timer interrupt (IRQ0 -> INT 32)
void init_timer_irq(void) {
    idt_set_entry(32, irq0, 0x8E);
    write_serial("IDT: Timer IRQ installed\n");
}

// Install keyboard interrupt (IRQ1 -> INT 33)
void init_keyboard_irq(void) {
    idt_set_entry(33, irq1, 0x8E);
    write_serial("IDT: Keyboard IRQ installed\n");
}

// Install syscall handler (INT 128)
void install_syscall(void) {
    idt_set_entry(128, isr128, 0x8E);  // 0x8E for kernel-mode only
    write_serial("IDT: Syscall handler installed\n");
}

// Exception handler
void isr_handler(uint64_t interrupt_number, uint64_t rip) {
    serial_printf("\n=== CPU EXCEPTION %lu ===\n", interrupt_number);

    switch(interrupt_number) {
        case 0:  serial_printf("Division Error\n"); break;
        case 6:  serial_printf("Invalid Opcode\n"); break;
        case 8:  serial_printf("Double Fault\n"); break;
        case 13: serial_printf("General Protection Fault\n"); break;
        case 14: {
            uint64_t fault_addr;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
            serial_printf("Page Fault at 0x%016lx\n", fault_addr);
            break;
        }
        default: serial_printf("Exception #%lu\n", interrupt_number); break;
    }

    serial_printf("RIP: 0x%016lx\n", rip);
    serial_printf("=== SYSTEM HALTED ===\n");
    
    __asm__ volatile ("cli; 1: hlt; jmp 1b");
    __builtin_unreachable();
}

// IRQ handler
void irq_handler(uint64_t irq_number) {
    switch(irq_number) {
        case 32: // Timer
            on_irq0();
            change_process();
            break;

        case 33: // Keyboard
            keyboard_handler(NULL);
            break;

        default:
            serial_printf("Unhandled IRQ: %lu\n", irq_number);
            break;
    }

    // Send EOI
    if (irq_number >= 40) {
        outb(0xA0, 0x20);  // Slave PIC
    }
    outb(0x20, 0x20);  // Master PIC
}
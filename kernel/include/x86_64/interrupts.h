#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

// IDT Entry structure (x86-64)
typedef struct {
    uint16_t isr_low;      // Lower 16 bits of ISR address
    uint16_t kernel_cs;    // Kernel code segment selector (0x08)
    uint8_t  ist;          // Interrupt Stack Table offset (0 for none)
    uint8_t  attributes;   // Type and attributes (0x8E for kernel interrupt)
    uint16_t isr_mid;      // Middle 16 bits of ISR address
    uint32_t isr_high;     // Upper 32 bits of ISR address
    uint32_t reserved;     // Reserved, must be 0
} __attribute__((packed)) idt_entry_t;

// IDT Pointer structure
typedef struct {
    uint16_t limit;        // Size of IDT - 1
    uint64_t base;         // Base address of IDT
} __attribute__((packed)) idtr_t;

// IDT functions
void idt_init(void);
void idt_load(void);
void install_exceptions(void);
void install_syscall(void);

// IRQ functions
void init_timer_irq(void);
void init_keyboard_irq(void);

// Handlers
void isr_handler(uint64_t interrupt_number, uint64_t rip);
void irq_handler(uint64_t irq_number);

#endif // INTERRUPTS_H
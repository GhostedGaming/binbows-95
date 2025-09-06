#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint16_t isr_low;      // Lower 16 bits of ISR address
    uint16_t kernel_cs;    // Kernel code segment selector
    uint8_t ist;           // Interrupt Stack Table offset
    uint8_t attributes;    // Type and attributes
    uint16_t isr_mid;      // Middle 16 bits of ISR address
    uint32_t isr_high;     // Upper 32 bits of ISR address
    uint32_t reserved;     // Reserved, must be 0
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;        // Size of IDT - 1
    uint64_t base;         // Base address of IDT
} __attribute__((packed)) idtr_t;

void idt_init(void);
void idt_load(void);
void install_exceptions(void);
void init_timer_irq(void);
void init_keyboard_irq(void);
void idt_set_gate(uint8_t vector, uint64_t isr, uint16_t selector, uint8_t flags);
int install_irq_handler(uint8_t irq, void (*handler)(void*), void* ctx);
void install_irq_common(uint8_t irq_vector, void (*handler)(void));

#endif // IDT_H
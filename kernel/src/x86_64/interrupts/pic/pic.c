#include <pic.h>
#include <serial.h>

// Send End of Interrupt
void PIC_sendEOI(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

// Unmask (enable) an IRQ line
void IRQ_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    
    uint8_t value = inb(port) & ~(1 << irq);
    outb(port, value);
}

// Initialize PIC (remap to IRQ 32-47, enable timer and keyboard)
void init_pic(void) {
    // Initialize both PICs
    outb(0x20, 0x11);  // ICW1: Initialize + ICW4
    outb(0xA0, 0x11);
    
    // Remap: Master to 32, Slave to 40
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    
    // Tell PICs about each other
    outb(0x21, 0x04);  // Master has slave at IRQ2
    outb(0xA1, 0x02);  // Slave cascade identity
    
    // Set mode
    outb(0x21, 0x01);  // 8086 mode
    outb(0xA1, 0x01);
    
    // Mask all except timer (0) and keyboard (1)
    outb(0x21, 0xFC);  // 11111100 - IRQ0 and IRQ1 enabled
    outb(0xA1, 0xFF);  // All slave IRQs masked
    
    write_serial("PIC: Initialized\n");
}
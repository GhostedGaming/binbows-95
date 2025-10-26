#include <pic.h>
#include <serial.h>

void PIC_sendEOI(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void IRQ_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    
    uint8_t value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void IRQ_set_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    
    uint8_t value = inb(port) | (1 << irq);
    outb(port, value);
}

void init_pic(void) {
    // Save masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // Initialize both PICs
    outb(PIC1_COMMAND, 0x11);  // ICW1: Initialize + ICW4
    outb(PIC2_COMMAND, 0x11);
    
    // Remap: Master to 32, Slave to 40
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    
    // Tell PICs about each other
    outb(PIC1_DATA, 0x04);  // Master has slave at IRQ2
    outb(PIC2_DATA, 0x02);  // Slave cascade identity
    
    // Set mode
    outb(PIC1_DATA, 0x01);  // 8086 mode
    outb(PIC2_DATA, 0x01);
    
    // Mask all IRQs initially
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    
    write_serial("PIC: Initialized\n");
}
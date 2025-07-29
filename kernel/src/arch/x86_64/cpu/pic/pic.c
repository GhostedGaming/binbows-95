#include <pic.h>
#include <serial.h>

static uint16_t __pic_get_irq_reg(int ocw3) {
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

uint16_t pic_get_irr(void) {
    return __pic_get_irq_reg(PIC_READ_IRR);
}

uint16_t pic_get_isr(void) {
    return __pic_get_irq_reg(PIC_READ_ISR);
}

void PIC_sendEOI(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);

    outb(PIC1_COMMAND, PIC_EOI);
}

void PIC_remap(int offset1, int offset2) {
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, offset1); // ICW2: Master PIC vector offset
    io_wait();
    outb(PIC2_DATA, offset2); // ICW2: Slave PIC vector offset
    io_wait();
    outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    io_wait();
    outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    outb(PIC1_DATA, ICW4_8086); // ICW4: have the PICs use 8086 mode (and not 8080 mode)
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Unmask both PICs.
    outb(PIC1_DATA, 0);
    outb(PIC2_DATA, 0);
}

void pic_disable(void) {
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}

void IRQ_set_mask(uint8_t IRQline) {
    uint16_t port;
    uint8_t value;

    if (IRQline < 8) {
        port = PIC1_DATA;
    }
    else {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) | (1 << IRQline);
    outb(port, value);
}

void IRQ_clear_mask(uint8_t IRQline) {
    uint16_t port;
    uint8_t value;

    if (IRQline < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) & ~(1 << IRQline);
    outb(port, value);
}

void pic_enable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = 0x21; // Master PIC
    } else {
        port = 0xA1; // Slave PIC
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_disable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = 0x21; // Master PIC
    } else {
        port = 0xA1; // Slave PIC
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void init_pic(void) {
    // Remap PIC interrupts
    outb(0x20, 0x11); // Initialize master PIC
    outb(0xA0, 0x11); // Initialize slave PIC
    
    outb(0x21, 0x20); // Master PIC offset (32)
    outb(0xA1, 0x28); // Slave PIC offset (40)
    
    outb(0x21, 0x04); // Tell master about slave at IRQ2
    outb(0xA1, 0x02); // Tell slave its cascade identity
    
    outb(0x21, 0x01); // 8086 mode
    outb(0xA1, 0x01); // 8086 mode
    
    // Enable both timer (IRQ0) and keyboard (IRQ1)
    outb(0x21, 0xFC); // Mask all except IRQ0 and IRQ1 (11111100)
    outb(0xA1, 0xFF); // Mask all slave IRQs
    
    write_serial("PIC: Initialized - Timer and Keyboard enabled\n");
}

void pic_enable_timer_irq(void) {
    // Enable IRQ0 (timer) in PIC
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 0); // Clear bit 0 to enable IRQ0
    outb(0x21, mask);
    
    write_serial("PIC: Timer IRQ0 enabled\n");
}
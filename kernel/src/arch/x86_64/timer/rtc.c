#include <io.h>
#include <pic.h>
#include <rtc.h>
#include <stdint.h>

static void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);
    }
    outb(PIC1_COMMAND, 0x20);
}

static void unmask_rtc_irq() {
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask = inb(PIC2_DATA);

    slave_mask &= ~(1 << (RTC_IRQ - 8));

    outb(PIC2_DATA, slave_mask);
    outb(PIC1_DATA, master_mask);
}

void init_rtc() {
    asm volatile ("cli");

    outb(0x70, 0x8A);
    outb(0x71, 0x26);

    outb(0x70, 0x8B);
    char prev = inb(0x71);
    outb(0x70, 0x8B);
    outb(0x71, prev | 0x40);

    unmask_rtc_irq();

    asm volatile ("sti");
}

void rtc_ack() {
    outb(0x70, 0x0C);
    (void) inb(0x71);
}

void rtc_handler() {
    rtc_ack();
    pic_send_eoi(RTC_IRQ);
}
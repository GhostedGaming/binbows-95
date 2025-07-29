#include <timer.h>
#include <serial.h>
#include <io.h>
#include <pic.h>

static volatile uint64_t timer_ticks = 0;

void on_irq0(void) {
    timer_ticks++;
    // Send EOI to PIC
    PIC_sendEOI(0);
}

void timer_wait(uint32_t ticks) {
    uint64_t start_ticks = timer_ticks;
    while (timer_ticks < start_ticks + ticks) {
        __asm__ volatile ("hlt");
    }
}

void init_timer(void) {
    // Set PIT frequency to ~100Hz (1193180 / 11932 ≈ 100)
    uint16_t divisor = 11932;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    
    write_serial("PIT: Timer initialized at ~100Hz\n");
}

void init_timer_interrupts(void) {
    pic_enable_timer_irq();
    write_serial("Timer: Interrupts initialized\n");
}

uint64_t get_timer_ticks(void) {
    return timer_ticks;
}
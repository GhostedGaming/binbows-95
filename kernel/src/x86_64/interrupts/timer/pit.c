#include <timer.h>
#include <serial.h>
#include <io.h>
#include <pic.h>

static volatile uint64_t timer_ticks = 0;

void on_irq0(void) {
    timer_ticks++;
    serial_printf(".");
}

void timer_wait(uint32_t ticks) {
    uint64_t start_ticks = timer_ticks;
    while (timer_ticks < start_ticks + ticks) {
        __asm__ volatile ("hlt");
    }
}

void init_timer(void) {
    uint16_t divisor = 11932;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    
    write_serial("PIT: Timer initialized at ~100Hz\n");
}

uint64_t get_timer_ticks(void) {
    return timer_ticks;
}
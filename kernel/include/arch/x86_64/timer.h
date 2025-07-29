#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void on_irq0(void);

void timer_wait(uint32_t ticks);

void init_timer(void);

void init_timer_interrupts(void);

uint64_t get_timer_ticks(void);

static inline void timer_wait_micros(uint32_t microseconds) {
    if (microseconds == 0) return;
    
    uint32_t ticks = (microseconds + 9999) / 10000;
    timer_wait(ticks);
}

static inline void timer_wait_ms(uint32_t milliseconds) {
    timer_wait(milliseconds / 10);
}

static inline void timer_wait_seconds(uint32_t seconds) {
    timer_wait(seconds * 100);
}

#endif
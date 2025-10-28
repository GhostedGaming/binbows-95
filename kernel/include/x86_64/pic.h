#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void init_pic(void);
void PIC_sendEOI(uint8_t irq);
void IRQ_set_mask(uint8_t irq);
void IRQ_clear_mask(uint8_t irq);

#endif
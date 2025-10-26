#ifndef PIC_H
#define PIC_H

#include <io.h>
#include <stdint.h>

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1
#define PIC_EOI         0x20

void init_pic(void);
void PIC_sendEOI(uint8_t irq);
void IRQ_clear_mask(uint8_t irq);

#endif
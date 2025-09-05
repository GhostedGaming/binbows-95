#ifndef RTC_H
#define RTC_H

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define RTC_IRQ 8

void init_rtc();
void rtc_ack();
void rtc_handler();

#endif
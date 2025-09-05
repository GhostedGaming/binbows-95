#ifndef RTC_H
#define RTC_H

#define RTC_IRQ 8

void init_rtc();
void rtc_ack();
void rtc_handler();

#endif
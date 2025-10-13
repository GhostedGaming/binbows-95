#ifndef KERNEL_H
#define KERNEL_H

#include <acpi.h>
#include <fat12.h>
#include <fat16.h>
#include <fat32.h>
#include <framebuffer.h>
#include <font.h>
#include <gdt.h>
#include <ide.h>
#include <interrupts.h>
#include <limine.h>
#include <mem.h>
#include <pci.h>
#include <pc_speaker.h>
#include <pic.h>
#include <rtc.h>
#include <serial.h>
#include <timer.h>
#include <enable_sse.h>
#include <scheduler.h>
#include <shell.h>
#include <sata.h>
#include <ps2_keyboard.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void kernel_main(void);

#endif // KERNEL_H
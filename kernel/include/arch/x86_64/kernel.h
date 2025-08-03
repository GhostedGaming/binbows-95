#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <serial.h>
#include <idt.h>
#include <gdt.h>
#include <pic.h>
#include <timer.h>
#include <pc_speaker.h>
#include <acpi.h>
#include <mem.h>
#include <ide.h>
#include <pci.h>
#include <uhci.h>
#include <framebuffer.h>
#include <font.h>
#include <rtc.h>
#include <fat12.h>

void kernel_main(void);

#endif // KERNEL_H
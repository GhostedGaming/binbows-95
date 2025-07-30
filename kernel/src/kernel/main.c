#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <kernel.h>
#include <serial.h>
#include <idt.h>
#include <gdt.h>
#include <pic.h>
#include <timer.h>
#include <pc_speaker.h>
#include <acpi.h>

void kernel_main(void) {

    // FUCK THE FRAMEBUFFER
    //if (LIMINE_BASE_REVISION_SUPPORTED == false) {
    //    hcf();
    //}
    //
    //if (framebuffer_request.response == NULL
    // || framebuffer_request.response->framebuffer_count < 1) {
    //    hcf();
    //}
    //
    //struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    //
    //for (size_t i = 0; i < 100; i++) {
    //    volatile uint32_t *fb_ptr = framebuffer->address;
    //    fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    //}

    init_serial();
    gdt_init();
    gdt_load();
    serial_printf("GDT loaded\r\n");

    idt_init();
    install_exceptions();
    init_timer_irq();
    init_keyboard_irq();
    idt_load();
    init_pic();

    //if (acpi_init() != 0) {
    //    write_serial("acpi_init failed");
    //} else {
//
    //}

    init_timer();
    asm volatile ("sti");

    beep(500, 25);

    while (1) {
        asm volatile ("hlt");
    }
}

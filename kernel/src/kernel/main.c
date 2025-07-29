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

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

void kmain(void) {

    // FUCK THE FRAMEBUFFER
    // we need interrupts
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

    gdt_init();
    gdt_load();
    serial_printf("GDT loaded\r\n");

    idt_init();
    install_exceptions();
    init_timer_irq();
    init_keyboard_irq();
    idt_load();
    init_pic();

    init_timer();
    asm volatile ("sti");

    beep(500, 25);

    hcf();
}

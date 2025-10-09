#include <kernel.h>
#include <scheduler.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_bootloader_info_request bootloader_request;

void kernel_main(void) {
    if (!memmap_request.response) while (1) asm volatile ("hlt");
    if (!hhdm_request.response) while (1) asm volatile ("hlt");

    enable_sse();
    init_serial();
    init_allocator();
    serial_printf("Linked List allocator initialized\n");

    gdt_init();
    gdt_load();
    serial_printf("GDT loaded\n");

    idt_init();
    install_exceptions();

    scheduler_init();

    init_timer_irq();
    init_keyboard_irq();
    idt_load();
    init_pic();
    init_timer();
    asm volatile ("sti");

    init_fb();

    ide_initialize();

    draw_text_center_screen("Hello world!", rgb_to_color(255, 255, 255));

    create_process(test_scheduler, 1);

    serial_printf("System initialized!\n");

    shell_init();

    while (1) {
        asm volatile ("hlt");
    }
}
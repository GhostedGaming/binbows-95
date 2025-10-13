#include <kernel.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_bootloader_info_request bootloader_request;

void shell_process(void);

void kernel_main(void) {
    if (!memmap_request.response) while (1) asm volatile ("hlt");
    if (!hhdm_request.response) while (1) asm volatile ("hlt");
    if (!bootloader_request.response) while (1) asm volatile ("hlt");

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

    idt_load();
    init_pic();

    init_timer_irq();
    init_keyboard_irq();
    init_timer();

    init_fb();

    ide_initialize();

    draw_text_center_screen("Hello world!", rgb_to_color(255, 255, 255));

    create_process(test_scheduler, 1);
    create_process(shell_process, 10);
    extern void keyboard_process(void);
    int kpid = create_process(keyboard_process, 5);
    if (kpid > 0) {
        keyboard_process_pid = (uint32_t)kpid;
    }
    shell_init();

    serial_printf("System initialized!\n");

    asm volatile ("sti");

    while (1) {
        asm volatile ("hlt");
    }
}
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
    sata_init();

    const char *msg = "Hello, SATA sector!";
    uint8_t write_buf[512] = {0};
    uint8_t read_buf[512] = {0};

    for (int i = 0; i < 512 && msg[i]; i++) {
        write_buf[i] = msg[i];
    }

    if (write_sectors(&abar->ports[0], 0, 0, 1, write_buf)) {
        serial_printf("Successfully wrote to sector 0\n");
    } else {
        serial_printf("Failed to write to sector 0\n");
    }

    if (read_sectors(&abar->ports[0], 0, 0, 1, read_buf)) {
        serial_printf("Read from sector 0: %s\n", read_buf);
    } else {
        serial_printf("Failed to read from sector 0\n");
    }

    create_process(test_scheduler, 1);
    create_process(shell_process, 10);
    shell_init();

    asm volatile ("sti");

    while (1) {
        asm volatile ("hlt");
    }
}

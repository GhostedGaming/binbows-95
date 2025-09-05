#include <kernel.h>

// LIMINE bootloader protocol requests
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_bootloader_info_request bootloader_request;

// IDE device array
extern struct ide_device ide_devices[4]; // Change the number for more ide drives

void kernel_main(void) {
     // Check memory map
    if (!memmap_request.response) {
        serial_printf("memmap_request.response is NULL!\n");
        while (1) asm volatile ("hlt");
    }

    // Check for hddm
    if (!hhdm_request.response) {
        serial_printf("hhdm_request.response is NULL!\n");
        while (1) asm volatile ("hlt");
    }

    // Initialize low-level systems
    enable_sse();
    init_serial();
    gdt_init();
    gdt_load();
    serial_printf("GDT loaded\n");

    // Install interrupts exceptions and IRQs
    idt_init();
    install_exceptions();
    init_timer_irq();
    init_keyboard_irq();
    idt_load();
    init_pic();

    init_timer();
    // Enable interrupts
    asm volatile ("sti");

    init_allocator();
    serial_printf("Linked List allocator initialized\n");

    ide_initialize();

    // Init framebuffer and PCI
    init_fb();
    draw_text(-1, -1, "Running PCI", rgb_to_color(255, 255, 255), true);
    serial_printf("Running PCI\n");

    check_all_buses();
    serial_printf("PCI finished!\n");

    draw_text(-1, -1, "System Initialized!", rgb_to_color(255, 255, 255), true);

    timer_wait_ms(150);

    // Initialize sad kernel shell :(
    shell_init();

    // Halt CPU
    while (1) asm volatile ("hlt");
}
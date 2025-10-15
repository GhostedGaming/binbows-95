#include <kernel.h>
#include <elixir.h>
#include <acpi.h>
#include <apic.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_bootloader_info_request bootloader_request;
extern HBA_MEM *abar;

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

    check_all_buses();
    ahci_init();

    acpi_init();
    apic_init();

    serial_printf("\n=== Formatting Elixir Filesystem ===\n");
    int format_result = elixir_format(&abar->ports[0], 65536 * 512);
    if (format_result != 0) {
        serial_printf("Failed to format Elixir filesystem: %d\n", format_result);
    } else {
        serial_printf("Elixir filesystem formatted successfully.\n");
        elixir_fs_t* fs = elixir_mount(&abar->ports[0]);
        if (!fs) {
            serial_printf("Failed to mount Elixir filesystem.\n");
        } else {
            serial_printf("Elixir filesystem mounted successfully.\n");
            elixir_unmount(fs);
        }
    }

    create_process(test_scheduler, 1);
    create_process(shell_process, 10);
    shell_init();

    write_serial("Enabling interrupts...\n");
    asm volatile ("sti");

    write_serial("Entering idle loop...\n");
    while (1) {
        asm volatile ("hlt");
    }
}
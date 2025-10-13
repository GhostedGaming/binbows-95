#include <kernel.h>
#include <elixir.h>

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

    ide_initialize();
    sata_init();

    serial_printf("\n=== Initializing Elixir Filesystem ===\n");
    struct elixir_init_data* init_data = init_elixir(&abar->ports[0]);
    if (!init_data) {
        serial_printf("Failed to initialize Elixir filesystem structures\n");
    } else {
        serial_printf("Elixir structures created successfully\n");
        
        int result = write_elixir_to_disk(&abar->ports[0], init_data);
        if (result != 0) {
            serial_printf("Failed to write Elixir filesystem to disk: %d\n", result);
        } else {
            serial_printf("Elixir filesystem written to disk successfully\n");
        }
        
        destroy_elixir_init_data(init_data);
    }

    create_process(test_scheduler, 1);
    create_process(shell_process, 10);
    shell_init();

    asm volatile ("sti");

    while (1) {
        asm volatile ("hlt");
    }
}
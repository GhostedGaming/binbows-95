#include <kernel.h>
#include <elixir.h>
#include <acpi.h>
#include <apic.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_bootloader_info_request bootloader_request;
extern HBA_MEM *abar;

extern void call_fb();

void kernel_main(void) {
    if (!memmap_request.response) while (1) asm volatile ("hlt");
    if (!hhdm_request.response) while (1) asm volatile ("hlt");
    if (!bootloader_request.response) while (1) asm volatile ("hlt");

    enable_sse();
    init_serial();
    init_allocator();
    
    gdt_init();
    gdt_load();

    idt_init();
    install_exceptions();
    install_syscall();

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

    create_process(test_scheduler, 1);
    create_process(shell_process, 10);
    shell_init();

    write_serial("Enabling interrupts...\n");
    __asm__ volatile ("sti");

    call_fb();

    write_serial("Entering idle loop...\n");
    while (1) {
        __asm__ volatile ("hlt");
    }
}
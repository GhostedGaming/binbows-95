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
    write_serial("Serial initialized\n");
    
    init_allocator();
    write_serial("Memory allocator initialized\n");
    
    gdt_init();
    gdt_load();
    write_serial("GDT loaded\n");

    idt_init();
    install_exceptions();
    install_syscall();
    idt_load();
    write_serial("IDT configured\n");
    
    init_pic();
    init_timer_irq();
    init_keyboard_irq();
    write_serial("PIC and IRQs configured\n");
    
    scheduler_init();
    init_timer();
    write_serial("Scheduler and timer initialized\n");

    init_keyboard();
    write_serial("Keyboard initialized\n");

    create_process(test_scheduler, 1);
    create_process(shell_process, 10);
    write_serial("Processes created\n");

     write_serial("Enabling interrupts...\n");
    __asm__ volatile ("sti");
    write_serial("Interrupts enabled!\n");

    write_serial("About to init framebuffer...\n");
    init_fb();
    write_serial("Framebuffer initialized\n");
    
    write_serial("About to check buses...\n");
    check_all_buses();
    write_serial("Buses checked\n");
    
    write_serial("About to init AHCI...\n");
    ahci_init();
    write_serial("AHCI initialized\n");

    write_serial("About to init IDE\n");
    ide_initialize();
    write_serial("IDE initialized\n");
    
    write_serial("About to init ACPI...\n");
    acpi_init();
    write_serial("ACPI initialized\n");
    
    write_serial("About to init APIC...\n");
    apic_init();
    write_serial("APIC initialized\n");

    write_serial("About to init shell...\n");
    shell_init();
    write_serial("Shell initialized\n");

    write_serial("About to call_fb...\n");
    call_fb();
    write_serial("Framebuffer activated\n");

    write_serial("Entering idle loop...\n");
    while (1) {
        __asm__ volatile ("hlt");
    }
}
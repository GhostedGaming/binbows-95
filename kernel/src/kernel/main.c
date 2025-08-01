#include <kernel.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_bootloader_info_request bootloader_request;

extern struct ide_device ide_devices[4];

void kernel_main(void) {
    init_serial();
    gdt_init(); gdt_load();
    serial_printf("GDT loaded\n");

    idt_init();
    install_exceptions();
    init_timer_irq();
    init_keyboard_irq();
    idt_load();
    init_pic();

    init_timer();
    asm volatile ("sti");
    init_rtc();

    if (!memmap_request.response) {
        serial_printf("memmap_request.response is NULL!\n");
        while (1) asm volatile ("hlt");
    }

    uintptr_t usable_phys_base = 0;
    size_t usable_length = 0;

    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap_request.response->entries[i];
        if (entry->type == 0 && entry->length > usable_length) {
            usable_phys_base = (uintptr_t)entry->base;
            usable_length = (size_t)entry->length;
        }
    }

    if (usable_phys_base == 0) {
        serial_printf("No usable memory found!\n");
        while (1) asm volatile ("hlt");
    }

    if (!hhdm_request.response) {
        serial_printf("hhdm_request.response is NULL!\n");
        while (1) asm volatile ("hlt");
    }

    uintptr_t hhdm_offset = (uintptr_t)hhdm_request.response->offset;
    void* usable_virt_base = (void*)(usable_phys_base + hhdm_offset);

    serial_printf("Using memory base: phys=0x%lx virt=0x%lx len=0x%lx\n",
        (unsigned long)usable_phys_base,
        (unsigned long)(uintptr_t)usable_virt_base,
        (unsigned long)usable_length);

    size_t early_alloc_size = 0x800000; // 8MB
    void* buddy_base = (void*)((uintptr_t)usable_virt_base + early_alloc_size);
    size_t buddy_size = usable_length - early_alloc_size;

    paging_init(usable_virt_base, early_alloc_size);
    serial_printf("Paging initialized\n");

    buddy_init(buddy_base, buddy_size);
    serial_printf("Buddy allocator initialized\n");

    if (acpi_init() != 0) {
        write_serial("ACPI initialization failed\n");
    }

    ide_initialize();

    if (bootloader_request.response) {
        serial_printf("Bootloader: %s %s\n",
            bootloader_request.response->name,
            bootloader_request.response->version);
    }

    init_fb();
    draw_text(-1, -1, "Formatting Disk...", rgb_to_color(255, 255, 255), false);

    draw_text(-1, -1, "Running PCI", rgb_to_color(255, 255, 255), true);
    serial_printf("Running PCI\n");

    check_all_buses();
    serial_printf("PCI finished!\n");

    draw_text(-1, -1, "Initiating UHCI", rgb_to_color(255, 255, 255), true);
    serial_printf("Running uhci_init\n");

    uhci_init();
    serial_printf("uhci_init finished!\n");

    draw_text(-1, -1, "System Initialized!", rgb_to_color(255, 255, 255), true);

    while (1) asm volatile ("hlt");
}
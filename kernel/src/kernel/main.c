#include <kernel.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_bootloader_info_request bootloader_request;

extern struct ide_device ide_devices[4];

void kernel_main(void) {
    init_serial();

    gdt_init();
    gdt_load();
    serial_printf("GDT loaded\n");

    idt_init();
    install_exceptions();
    init_timer_irq();
    init_keyboard_irq();
    idt_load();
    init_pic();

    init_timer();
    asm volatile ("sti");

    if (memmap_request.response == NULL) {
        serial_printf("memmap_request.response is NULL!\n");
        while (1) asm volatile ("hlt");
    }

    serial_printf("Memory map entries:\n");

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

    if (hhdm_request.response == NULL) {
        serial_printf("hhdm_request.response is NULL!\n");
        while (1) asm volatile ("hlt");
    }

    uintptr_t hhdm_offset = (uintptr_t)hhdm_request.response->offset;
    void* usable_virt_base = (void*)(usable_phys_base + hhdm_offset);

    serial_printf("Using usable memory base (phys)=0x%lx (virt)=0x%lx length=0x%lx\n",
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

    uint32_t total_sectors = ide_devices[0].Size;
    serial_printf("Formatting drive 0 (%u sectors) as FAT16...\n", total_sectors);

    int res = fat16_format(total_sectors);
    if (res != 0) {
        serial_printf("Failed to format drive 0 as FAT16! Error code: %d\n", res);
        while (1) asm volatile ("hlt");
    } else {
        serial_printf("Drive 0 formatted successfully as FAT16.\n");
    }

    uint8_t sector[512] = { 'H', 'e', 'l', 'l', 'o', '!', 0 };
    if (ide_write_sectors(0, 1, 1, sector) != 0) {
        serial_printf("Failed to write to sector 1!\n");
    } else {
        if (ide_read_sectors(0, 1, 1, sector) != 0) {
            serial_printf("Failed to read sector 1!\n");
        } else {
            for (int i = 0; i < 64; i++) {
                serial_printf("%02X ", sector[i]);
                if ((i + 1) % 16 == 0) serial_printf("\n");
            }
        }
    }

    serial_printf("Running PCI\n");
    check_all_buses();
    serial_printf("PCI finished!");

    while (1) asm volatile ("hlt");
}
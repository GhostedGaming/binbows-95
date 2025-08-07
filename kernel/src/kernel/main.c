#include <kernel.h>
#include <test.h>
#include <fbe.h>

// LIMINE bootloader protocol requests
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_bootloader_info_request bootloader_request;

// IDE device array
extern struct ide_device ide_devices[4]; // Change the number for more ide drives

// Global execution memory region
void* global_exec_region = NULL;
size_t global_exec_size = 0x100000; // 1MB for binary execution

/**
 * Runs a series of tests to verify FAT12 file operations.
 */
/*void test_file_operations(uint8_t drive) {
    serial_printf("\n=== Testing FAT12 File Operations ===\n");

    // Test 1: Write and read a small text file
    const char *test_msg1 = "Hello, FAT12 world!";
    serial_printf("Writing file: HELLO.TXT\n");
    if (fat12_write_file(drive, "HELLO.TXT", (const uint8_t *)test_msg1, strlen(test_msg1)) == 0) {
        serial_printf("File write successful\n");

        uint8_t buffer[512];
        uint32_t size = 0;
        if (fat12_read_file(drive, "HELLO.TXT", buffer, &size)) {
            serial_printf("File read successful. Content: ");
            for (uint32_t i = 0; i < size; i++) write_serial_char(buffer[i]);
            write_serial_char('\n');
        } else {
            serial_printf("File read failed\n");
        }
    } else {
        serial_printf("File write failed\n");
    }

    // Test 2: Write and read a binary file
    uint8_t binary_data[256];
    for (int i = 0; i < 256; i++) binary_data[i] = i;

    serial_printf("\nWriting binary file: DATA.BIN\n");
    if (fat12_write_file(drive, "DATA.BIN", binary_data, 256) == 0) {
        serial_printf("Binary file write successful\n");

        uint8_t buffer[512];
        uint32_t size = 0;
        if (fat12_read_file(drive, "DATA.BIN", buffer, &size)) {
            serial_printf("Binary file read successful. Size: %u bytes\n", size);
            serial_printf("Binary file hex + ASCII dump:\n");

            for (uint32_t i = 0; i < size; i += 16) {
                serial_printf("%04X: ", i);
                for (uint32_t j = 0; j < 16; j++)
                    serial_printf(i + j < size ? "%02X " : "   ", buffer[i + j]);

                serial_printf(" |");
                for (uint32_t j = 0; j < 16; j++) {
                    uint8_t c = (i + j < size) ? buffer[i + j] : ' ';
                    write_serial_char((c >= 32 && c <= 126) ? c : '.');
                }
                serial_printf("|\n");
            }

            bool data_ok = true;
            for (uint32_t i = 0; i < size; i++) {
                if (buffer[i] != (uint8_t)i) { data_ok = false; break; }
            }
            serial_printf("Data integrity check: %s\n", data_ok ? "PASSED" : "FAILED");
        } else {
            serial_printf("Binary file read failed\n");
        }
    } else {
        serial_printf("Binary file write failed\n");
    }

    // Test 3: Write and read a large multi-cluster file
    const char *large_msg =
        "This is a larger file that should span multiple clusters. "
        "We're testing the cluster chaining functionality of our FAT12 "
        "implementation. If you can read this entire message, then "
        "the multi-cluster file handling is working correctly. "
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
        "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco "
        "laboris nisi ut aliquip ex ea commodo consequat.";

    serial_printf("\nWriting large file: LARGE.TXT\n");
    if (fat12_write_file(drive, "LARGE.TXT", (const uint8_t *)large_msg, strlen(large_msg)) == 0) {
        serial_printf("Large file write successful\n");

        static uint8_t large_buffer[1024];
        uint32_t large_size = 0;

        if (fat12_read_file(drive, "LARGE.TXT", large_buffer, &large_size)) {
            serial_printf("Large file read successful. Size: %u bytes\n", large_size);
            serial_printf("Content preview: ");
            for (uint32_t i = 0; i < (large_size > 64 ? 64 : large_size); i++)
                write_serial_char(large_buffer[i]);
            if (large_size > 64) serial_printf("...");
            write_serial_char('\n');

            serial_printf("Hex dump with ASCII:\n");
            for (uint32_t i = 0; i < large_size; i += 16) {
                serial_printf("%04X: ", i);
                for (uint32_t j = 0; j < 16; j++)
                    serial_printf(i + j < large_size ? "%02X " : "   ", large_buffer[i + j]);

                serial_printf(" |");
                for (uint32_t j = 0; j < 16; j++) {
                    uint8_t c = (i + j < large_size) ? large_buffer[i + j] : ' ';
                    write_serial_char((c >= 32 && c <= 126) ? c : '.');
                }
                serial_printf("|\n");
            }
        } else {
            serial_printf("Large file read failed\n");
        }
    } else {
        serial_printf("Large file write failed\n");
    }

    serial_printf("\n=== File Operations Test Complete ===\n");
} */

/**
 * Kernel entry point. Initializes all systems.
 */
void kernel_main(void) {
    // Initialize low-level systems
    enable_sse();
    init_serial();
    gdt_init();
    gdt_load();
    serial_printf("GDT loaded\n");

    idt_init();
    syscall_init();
    install_exceptions();
    init_timer_irq();
    init_keyboard_irq();
    idt_load();
    init_pic();

    init_timer();
    asm volatile ("sti");
    init_rtc();

    // Check memory map
    if (!memmap_request.response) {
        serial_printf("memmap_request.response is NULL!\n");
        while (1) asm volatile ("hlt");
    }

    // Find largest usable memory region
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

    // Initialize paging and buddy allocator
    size_t early_alloc_size = 0x800000;
    void* buddy_base = (void*)((uintptr_t)usable_virt_base + early_alloc_size);
    size_t buddy_size = usable_length - early_alloc_size;

    paging_init(usable_virt_base, early_alloc_size);
    serial_printf("Paging initialized\n");

    buddy_init(buddy_base, buddy_size);
    serial_printf("Buddy allocator initialized\n");

    // *** NEW: Allocate dedicated execution memory region ***
    global_exec_region = buddy_alloc(global_exec_size);
    if (!global_exec_region) {
        serial_printf("CRITICAL: Failed to allocate execution memory region (%zu bytes)!\n", global_exec_size);
        while (1) asm volatile ("hlt");
    }
    serial_printf("Execution region allocated: %p (size: %zu bytes)\n", global_exec_region, global_exec_size);

    ide_initialize();

    // Init framebuffer and PCI
    init_fb();
    draw_text(-1, -1, "Running PCI", rgb_to_color(255, 255, 255), true);
    serial_printf("Running PCI\n");
    check_all_buses();
    serial_printf("PCI finished!\n");

    // Init USB UHCI
    draw_text(-1, -1, "Initiating UHCI", rgb_to_color(255, 255, 255), true);
    serial_printf("Running uhci_init\n");
    uhci_init();
    serial_printf("uhci_init finished!\n");

    draw_text(-1, -1, "System Initialized!", rgb_to_color(255, 255, 255), true);

    timer_wait_seconds(5);

    shell_init();

    uint64_t rax_value;

    __asm__ volatile ("mov %%rax, %0" : "=r"(rax_value));

    serial_printf("RAX = 0x%016lx\n", rax_value);

    // Halt CPU
    for (;;) asm volatile ("hlt");
}
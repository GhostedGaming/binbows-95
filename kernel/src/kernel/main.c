#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <kernel.h>
#include <serial.h>
#include <idt.h>
#include <gdt.h>
#include <pic.h>
#include <timer.h>
#include <pc_speaker.h>
#include <acpi.h>
#include <memory.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

void kernel_main(void) {
    // Setup serial
    init_serial();
    
    // Initialize GDT
    gdt_init();
    gdt_load();
    serial_printf("GDT loaded\n");
    
    // Setup the IDT with all its interrupts
    idt_init();
    install_exceptions();
    init_timer_irq();
    init_keyboard_irq();
    idt_load();
    init_pic();
    
    // Initialize PIT
    init_timer();
    asm volatile ("sti");
    
    // Check if memory map is available
    if (memmap_request.response == NULL) {
        serial_printf("memmap_request.response is NULL!\n");
        while(1) asm volatile ("hlt");
    }
    
    // Print memory map for debugging
    serial_printf("Memory map entries:\n");
    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap_request.response->entries[i];
        serial_printf("Entry %u: base=0x%lx length=0x%lx type=%u\n",
                     (unsigned int)i,
                     (unsigned long)entry->base,
                     (unsigned long)entry->length,
                     (unsigned int)entry->type);
    }
    
    // Find the largest usable memory region for buddy allocator
    uintptr_t usable_phys_base = 0;
    size_t usable_length = 0;
    
    for (size_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap_request.response->entries[i];
        
        // Look for USABLE memory (type 0), not reserved (type 1)!
        if (entry->type == 0 && entry->length > usable_length) {
            usable_phys_base = (uintptr_t)entry->base;
            usable_length = (size_t)entry->length;
        }
    }
    
    if (usable_phys_base == 0) {
        serial_printf("No usable memory found!\n");
        while (1) asm volatile ("hlt");
    }
    
    // Check HHDM
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
    
    // Reserve more memory for early page table allocation
    // Conservative approach: map only essential regions
    size_t early_alloc_size = 0x800000; // 8MB should be enough for essential mappings
    void* buddy_base = (void*)((uintptr_t)usable_virt_base + early_alloc_size);
    size_t buddy_size = usable_length - early_alloc_size;
    
    // Initialize paging first (uses early allocator)
    paging_init(usable_virt_base, early_alloc_size);
    serial_printf("Paging initialized\n");
    
    // Now initialize buddy allocator with remaining memory
    buddy_init(buddy_base, buddy_size);
    serial_printf("Buddy allocator initialized\n");
    
    beep(500, 25);
    
    while (1) asm volatile ("hlt");
}
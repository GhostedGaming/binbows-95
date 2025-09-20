#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <mem.h>
#include <serial.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

static free_list_block *free_list_head = NULL;

#define MIN_ALLOC_SIZE 16
#define ALIGN_SIZE 16

uint32_t page_directory[1024] __attribute__((aligned(4096)));

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_request.response->offset);
}

static size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Save interrupt flag and disable interrupts. Returns the original RFLAGS.
static inline unsigned long save_and_cli(void) {
    unsigned long flags;
    asm volatile("pushfq; pop %0" : "=r" (flags) :: "memory");
    asm volatile("cli" ::: "memory");
    return flags;
}

// Restore interrupt state based on saved RFLAGS
static inline void restore_flags(unsigned long flags) {
    if (flags & (1UL << 9)) {
        asm volatile("sti" ::: "memory");
    }
}

// Insert block into free list sorted by virtual address (ascending)
static void insert_free_block_sorted(free_list_block *block) {
    if (!free_list_head || block < free_list_head) {
        block->next = free_list_head;
        free_list_head = block;
        return;
    }

    free_list_block *cur = free_list_head;
    while (cur->next && cur->next < block) cur = cur->next;
    block->next = cur->next;
    cur->next = block;
}

// Coalesce adjacent free blocks (assumes list sorted by address)
static void coalesce_free_list(void) {
    free_list_block *cur = free_list_head;
    while (cur && cur->next) {
        uint8_t *cur_end = (uint8_t *)cur + sizeof(free_list_block) + cur->size;
        if (cur_end == (uint8_t *)cur->next) {
            // adjacent: merge cur and cur->next
            cur->size += sizeof(free_list_block) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void init_allocator() {
    if (!memmap_request.response) {
        serial_printf("Memmap response was null\n");
        return;
    }
    if (!hhdm_request.response) {
        serial_printf("HHDM not available; map memory or use an identity-mapped bump allocator first.\n");
        return;
    }

    uint64_t hhdm = hhdm_request.response->offset;
    struct limine_memmap_response *memmap = memmap_request.response;

    uint64_t total_ram = 0;
    serial_printf("Looping through memory\n");
    for (size_t i = 0; i < memmap->entry_count; ++i) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        serial_printf("Entry %lu: base=0x%lx, length=%lu, type=%lu\n",
                      i, entry->base, entry->length, entry->type);
        if (entry->type == LIMINE_MEMMAP_USABLE) total_ram += entry->length;
    }
    serial_printf("Memory: %lu MiB\n", total_ram / (1024 * 1024));

    free_list_head = NULL;
    int blocks_added = 0;

    for (size_t i = 0; i < memmap->entry_count; ++i) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE &&
            entry->length > (1 * 1024 * 1024) &&
            entry->base >= 0x00100000) {

            uint64_t phys_start = (entry->base + ALIGN_SIZE - 1) & ~(uint64_t)(ALIGN_SIZE - 1);
            uint64_t phys_end   = entry->base + entry->length;
            if (phys_end <= phys_start + sizeof(free_list_block)) continue;

            uint64_t usable_len = phys_end - phys_start;
            serial_printf("Using region: base=0x%lx, length=%lu\n", phys_start, usable_len);

            // Place the first free_list_block header at the *virtual* address
            free_list_block *block = (free_list_block *)phys_to_virt(phys_start);
            block->size = usable_len - sizeof(free_list_block);
            block->next = NULL;
            insert_free_block_sorted(block);
            blocks_added++;

            serial_printf("Added block %d: vaddr=0x%lx (phys=0x%lx) size=%lu\n",
                          blocks_added, (uint64_t)block, phys_start, block->size);
        }
    }

    if (blocks_added == 0) {
        serial_printf("No usable blocks found\n");
        return;
    }

    // coalesce adjacent regions we may have just inserted
    coalesce_free_list();

    serial_printf("Allocator initialized with %d blocks (HHDM=0x%lx)\n", blocks_added, hhdm);
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    if (free_list_head == NULL) return NULL;

    size = align_up(size, ALIGN_SIZE);
    if (size < MIN_ALLOC_SIZE) size = MIN_ALLOC_SIZE;

    unsigned long flags = save_and_cli();
    free_list_block **current = &free_list_head;

    while (*current != NULL) {
        free_list_block *block = *current;

        if (block->size >= size) {
            if (block->size >= size + sizeof(free_list_block) + MIN_ALLOC_SIZE) {
                free_list_block *new_block = (free_list_block *)((uint8_t *)block + sizeof(free_list_block) + size);
                new_block->size = block->size - size - sizeof(free_list_block);
                new_block->next = block->next;

                block->size = size;
                block->next = new_block;
            }

            *current = block->next; // remove allocated block (or replace with remainder)
            restore_flags(flags);
            return (uint8_t *)block + sizeof(free_list_block);
        }

        current = &(block->next);
    }

    restore_flags(flags);
    return NULL;
}

void kfree(void *ptr) {
    if (ptr == NULL) return;

    free_list_block *block = (free_list_block *)((uint8_t *)ptr - sizeof(free_list_block));

    unsigned long flags = save_and_cli();

    // insert sorted and coalesce with neighbors
    insert_free_block_sorted(block);

    // coalesce possibly with previous/next
    coalesce_free_list();

    restore_flags(flags);
}

void debug_free_list() {
    serial_printf("=== Free List ===\n");

    if (free_list_head == NULL) {
        serial_printf("Empty\n");
        return;
    }

    free_list_block *current = free_list_head;
    int count = 0;

    while (current != NULL && count < 40) {
        serial_printf("Block %d: 0x%lx size=%lu next=0x%lx\n",
                     count, (uint64_t)current, current->size, (uint64_t)current->next);
        current = current->next;
        count++;
    }

    serial_printf("=== End ===\n");
}

void print_memory_stats() {
    size_t total_free = 0;
    int block_count = 0;
    free_list_block *current = free_list_head;

    while (current != NULL) {
        total_free += current->size;
        block_count++;
        current = current->next;
    }

    serial_printf("Stats: %d blocks, %lu bytes free\n", block_count, total_free);
}

void test_allocator() {
    serial_printf("=== Test ===\n");

    void *ptr1 = kmalloc(64);
    serial_printf("Alloc 64: 0x%lx\n", (uint64_t)ptr1);

    void *ptr2 = kmalloc(128);
    serial_printf("Alloc 128: 0x%lx\n", (uint64_t)ptr2);

    kfree(ptr1);
    serial_printf("Freed first\n");

    kfree(ptr2);
    serial_printf("Freed second\n");

    print_memory_stats();
    serial_printf("=== End Test ===\n");
}
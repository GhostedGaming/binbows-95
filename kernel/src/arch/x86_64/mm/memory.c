#include <stdint.h>
#include <limine.h>
#include <memory.h>
#include <serial.h>

//====================Limine stuff===================================
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;

//====================Paging Constants==============================
#define PAGE_SIZE 0x1000
#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4
#define HHDM_OFFSET (uint64_t)hhdm_request.response->offset

//====================Memory Map Types===============================
#define LIMINE_MEMMAP_USABLE 0
#define LIMINE_MEMMAP_RESERVED 1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE 2
#define LIMINE_MEMMAP_ACPI_NVS 3
#define LIMINE_MEMMAP_BAD_MEMORY 4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES 6
#define LIMINE_MEMMAP_FRAMEBUFFER 7

//====================Early Memory Allocator========================
static uint8_t* early_alloc_base = NULL;
static size_t early_alloc_size = 0;
static size_t early_alloc_used = 0;

static void early_alloc_init(void* base, size_t size) {
    early_alloc_base = (uint8_t*)base;
    early_alloc_size = size;
    early_alloc_used = 0;
    serial_printf("Early allocator initialized: base=0x%lx size=0x%lx\n", 
                 (uint64_t)base, size);
}

static void* early_alloc(size_t size) {
    // Align to page boundary
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    if (early_alloc_used + size > early_alloc_size) {
        serial_printf("Early allocator out of memory! Used: 0x%lx, Requested: 0x%lx, Total: 0x%lx\n",
                     early_alloc_used, size, early_alloc_size);
        return NULL;
    }
    
    void* result = early_alloc_base + early_alloc_used;
    early_alloc_used += size;
    
    memset(result, 0, size);
    return result;
}

//====================Paging Helper Functions========================
static inline void load_cr3(uint64_t pml4_phys) {
    asm volatile ("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

static uint64_t* alloc_page_table(void) {
    void* page = early_alloc(PAGE_SIZE);
    if (!page) return NULL;
    return (uint64_t*)page;
}

static void map_page(uint64_t* pml4, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    uint64_t hhdm_offset = (uint64_t)hhdm_request.response->offset;

    int pml4_i = (virt_addr >> 39) & 0x1FF;
    int pdpt_i = (virt_addr >> 30) & 0x1FF;
    int pd_i   = (virt_addr >> 21) & 0x1FF;
    int pt_i   = (virt_addr >> 12) & 0x1FF;

    if (!(pml4[pml4_i] & PAGE_PRESENT)) {
        uint64_t* pdpt = alloc_page_table();
        if (!pdpt) {
            serial_printf("Failed to allocate PDPT\n");
            while(1) asm volatile ("hlt");
        }
        pml4[pml4_i] = ((uint64_t)pdpt - hhdm_offset) | flags;
    }
    uint64_t* pdpt = (uint64_t*)(hhdm_offset + (pml4[pml4_i] & ~0xFFFUL));

    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
        uint64_t* pd = alloc_page_table();
        if (!pd) {
            serial_printf("Failed to allocate PD\n");
            while(1) asm volatile ("hlt");
        }
        pdpt[pdpt_i] = ((uint64_t)pd - hhdm_offset) | flags;
    }
    uint64_t* pd = (uint64_t*)(hhdm_offset + (pdpt[pdpt_i] & ~0xFFFUL));

    if (!(pd[pd_i] & PAGE_PRESENT)) {
        uint64_t* pt = alloc_page_table();
        if (!pt) {
            serial_printf("Failed to allocate PT\n");
            while(1) asm volatile ("hlt");
        }
        pd[pd_i] = ((uint64_t)pt - hhdm_offset) | flags;
    }
    uint64_t* pt = (uint64_t*)(hhdm_offset + (pd[pd_i] & ~0xFFFUL));

    pt[pt_i] = phys_addr | flags;
}

//====================Find Usable Memory=============================
typedef struct {
    uint64_t base;
    uint64_t length;
} usable_region_t;

static usable_region_t find_largest_usable_region(void) {
    struct limine_memmap_response* memmap = memmap_request.response;
    uint64_t best_base = 0;
    uint64_t best_length = 0;
    
    serial_printf("Searching for usable memory regions:\n");
    
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        
        serial_printf("Entry %lu: base=0x%lx length=0x%lx type=%lu\n", 
                     i, entry->base, entry->length, entry->type);
        
        // Only use actually usable memory (type 0)
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length > best_length) {
            best_base = entry->base;
            best_length = entry->length;
        }
    }
    
    serial_printf("Selected usable region: base=0x%lx length=0x%lx\n", best_base, best_length);
    return (usable_region_t){best_base, best_length};
}

//====================Paging Initialization==========================
void paging_init(void* early_mem_base, size_t early_mem_size) {
    uint64_t hhdm_offset = (uint64_t)hhdm_request.response->offset;
    
    serial_printf("HHDM offset: 0x%lx\n", hhdm_offset);
    
    // Get current CR3 to see what Limine set up
    uint64_t current_cr3;
    asm volatile ("mov %%cr3, %0" : "=r"(current_cr3));
    serial_printf("Current CR3 (from Limine): 0x%lx\n", current_cr3);
    
    // Access current PML4 through HHDM
    uint64_t* limine_pml4 = (uint64_t*)(current_cr3 + hhdm_offset);
    serial_printf("Limine PML4 at: 0x%lx\n", (uint64_t)limine_pml4);
    
    // Initialize early allocator for page tables
    early_alloc_init(early_mem_base, early_mem_size);
    
    uint64_t* pml4 = alloc_page_table();
    if (!pml4) {
        serial_printf("Page table allocation failed\n");
        while(1) asm volatile ("hlt");
    }

    serial_printf("New PML4 allocated at (virt): 0x%lx\n", (uint64_t)pml4);

    // Copy Limine's PML4 entries to preserve existing mappings
    serial_printf("Copying Limine's page table entries...\n");
    for (int i = 0; i < 512; i++) {
        pml4[i] = limine_pml4[i];
        if (pml4[i] & PAGE_PRESENT) {
            serial_printf("PML4[%d] = 0x%lx (copied from Limine)\n", i, pml4[i]);
        }
    }

    // Map essential low memory (identity mapping)
    serial_printf("Adding identity mappings for low memory...\n");
    for (uint64_t addr = 0; addr < 0x100000; addr += PAGE_SIZE) {
        map_page(pml4, addr, addr, PAGE_PRESENT | PAGE_WRITE);
    }

    // Ensure our buddy allocator memory is mapped
    serial_printf("Ensuring buddy allocator region is mapped...\n");
    uint64_t buddy_start = (uint64_t)early_mem_base;
    uint64_t buddy_end = buddy_start + early_mem_size + 0x1000000; // Add 16MB buffer
    
    for (uint64_t virt = buddy_start; virt < buddy_end; virt += PAGE_SIZE) {
        uint64_t phys = virt - hhdm_offset;
        map_page(pml4, virt, phys, PAGE_PRESENT | PAGE_WRITE);
    }

    serial_printf("About to load CR3...\n");
    
    // Load CR3 with physical address of PML4
    uint64_t pml4_phys = (uint64_t)pml4 - hhdm_offset;
    serial_printf("Loading CR3 with PML4 physical address: 0x%lx\n", pml4_phys);
    
    load_cr3(pml4_phys);
    
    serial_printf("CR3 loaded successfully!\n");
    serial_printf("Paging initialized successfully\n");
    serial_printf("Early allocator used: 0x%lx / 0x%lx bytes\n", 
                 early_alloc_used, early_alloc_size);
}

//====================Memory utilization functions====================
void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;
    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }
    return 0;
}

//==========================Buddy Allocator==========================
#define MIN_ORDER 12  // 4 KiB
#define MAX_ORDER 20  // 1 MiB
#define MAX_BLOCKS (MAX_ORDER + 1)

typedef struct BuddyBlock {
    struct BuddyBlock* next;
} BuddyBlock;

static BuddyBlock* free_lists[MAX_BLOCKS];
static uint8_t* buddy_base;

static int get_order(size_t size) {
    size_t total = size;
    int order = MIN_ORDER;
    while ((1UL << order) < total && order <= MAX_ORDER)
        order++;
    return order;
}

static void* get_buddy(void* addr, int order) {
    uintptr_t offset = (uintptr_t)addr - (uintptr_t)buddy_base;
    uintptr_t buddy_offset = offset ^ (1UL << order);
    return (void*)((uintptr_t)buddy_base + buddy_offset);
}

void buddy_init(void* base, size_t length) {
    buddy_base = (uint8_t*)base;

    for (int i = 0; i < MAX_BLOCKS; i++)
        free_lists[i] = NULL;

    uintptr_t aligned_base = (uintptr_t)base;
    size_t aligned_size = length & ~((1UL << MIN_ORDER) - 1);

    int order = MAX_ORDER;
    while ((1UL << order) > aligned_size)
        order--;

    BuddyBlock* block = (BuddyBlock*)aligned_base;
    block->next = NULL;
    free_lists[order] = block;
}

void* buddy_alloc(size_t size) {
    int order = get_order(size);
    int current = order;

    while (current <= MAX_ORDER && free_lists[current] == NULL)
        current++;

    if (current > MAX_ORDER)
        return NULL;

    while (current > order) {
        BuddyBlock* block = free_lists[current];
        free_lists[current] = block->next;
        current--;

        uintptr_t addr = (uintptr_t)block;
        BuddyBlock* buddy = (BuddyBlock*)(addr + (1UL << current));
        buddy->next = NULL;

        block->next = NULL;
        free_lists[current] = buddy;
    }

    BuddyBlock* block = free_lists[order];
    free_lists[order] = block->next;
    return (void*)block;
}

void buddy_free(void* ptr, size_t size) {
    int order = get_order(size);
    uintptr_t addr = (uintptr_t)ptr;

    while (order <= MAX_ORDER) {
        void* buddy = get_buddy((void*)addr, order);

        BuddyBlock** current = &free_lists[order];
        BuddyBlock* prev = NULL;

        while (*current) {
            if (*current == (BuddyBlock*)buddy) {
                if (prev)
                    prev->next = (*current)->next;
                else
                    free_lists[order] = (*current)->next;

                if ((uintptr_t)buddy < addr)
                    addr = (uintptr_t)buddy;

                order++;
                goto try_merge;
            }
            prev = *current;
            current = &(*current)->next;
        }
        break;
    try_merge:;
    }

    BuddyBlock* block = (BuddyBlock*)addr;
    block->next = free_lists[order];
    free_lists[order] = block;
}
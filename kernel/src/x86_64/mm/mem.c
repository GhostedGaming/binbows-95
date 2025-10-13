#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <mem.h>
#include <serial.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_kernel_address_request kernel_address_request;

static free_list_block *free_list_head = NULL;

#define MIN_ALLOC_SIZE 16
#define ALIGN_SIZE 16
#define PAGE_SIZE 4096
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_USER (1ULL << 2)

typedef struct {
    uint64_t entries[512];
} __attribute__((aligned(4096))) page_table_t;

static page_table_t *pml4 = NULL;
static uint64_t next_free_page = 0;
static uint64_t max_physical_addr = 0;
static uint64_t paging_pool_start = 0;
static uint64_t paging_pool_end = 0;

uint32_t page_directory[1024] __attribute__((aligned(4096)));

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_request.response->offset);
}

static inline uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - hhdm_request.response->offset;
}

static size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static inline unsigned long save_and_cli(void) {
    unsigned long flags;
    __asm__ volatile("pushfq; pop %0" : "=r" (flags) :: "memory");
    __asm__ volatile("cli" ::: "memory");
    return flags;
}

static inline void restore_flags(unsigned long flags) {
    if (flags & (1UL << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

static inline void invlpg(uint64_t vaddr) {
    __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

static inline void load_cr3(uint64_t pml4_phys) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static void memset_page(void *ptr, int value, size_t size) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = (uint8_t)value;
    }
}

static uint64_t free_phys_list = 0;

static uint64_t alloc_physical_page(void) {
    if (free_phys_list != 0) {
        uint64_t phys = free_phys_list;
        free_phys_list = *(uint64_t *)phys_to_virt(phys);
        void *virt = phys_to_virt(phys);
        memset_page(virt, 0, PAGE_SIZE);
        return phys;
    }

    if (next_free_page == 0 || next_free_page >= paging_pool_end) {
        return 0;
    }

    uint64_t page = next_free_page;
    next_free_page += PAGE_SIZE;

    void *virt = phys_to_virt(page);
    memset_page(virt, 0, PAGE_SIZE);

    return page;
}

static void free_physical_page(uint64_t phys) {
    if (phys == 0) return;
    if (phys < paging_pool_start || phys >= paging_pool_end) return;
    void *virt = phys_to_virt(phys);
    *(uint64_t *)virt = free_phys_list;
    free_phys_list = phys;
}

static page_table_t *get_or_create_table(uint64_t *entry) {
    if (*entry & PAGE_PRESENT) {
        uint64_t phys = *entry & ~0xFFFULL;
        return (page_table_t *)phys_to_virt(phys);
    }
    
    uint64_t phys = alloc_physical_page();
    if (phys == 0) return NULL;
    
    *entry = phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    return (page_table_t *)phys_to_virt(phys);
}

int map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;
    
    page_table_t *pdpt = get_or_create_table(&pml4->entries[pml4_idx]);
    if (!pdpt) return -1;
    
    page_table_t *pd = get_or_create_table(&pdpt->entries[pdpt_idx]);
    if (!pd) return -1;
    
    page_table_t *pt = get_or_create_table(&pd->entries[pd_idx]);
    if (!pt) return -1;
    
    pt->entries[pt_idx] = (paddr & ~0xFFFULL) | flags;
    
    return 0;
}

static void copy_page_table_entries(page_table_t *dest, page_table_t *src) {
    for (int i = 0; i < 512; i++) {
        dest->entries[i] = src->entries[i];
    }
}

void init_paging(void) {
    struct limine_memmap_response *memmap = memmap_request.response;
    
    uint64_t old_cr3 = read_cr3();
    page_table_t *old_pml4 = (page_table_t *)phys_to_virt(old_cr3 & ~0xFFFULL);
    
    serial_printf("Current CR3: 0x%lx\n", old_cr3);
    
    max_physical_addr = 0;
    uint64_t kernel_phys_end = 0;
    
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        uint64_t end = entry->base + entry->length;
        if (end > max_physical_addr) {
            max_physical_addr = end;
        }
        if (entry->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES) {
            uint64_t k_end = entry->base + entry->length;
            if (k_end > kernel_phys_end) {
                kernel_phys_end = k_end;
            }
        }
    }
    
    uint64_t map_limit = max_physical_addr;
    if (map_limit > 0x100000000ULL) {
        map_limit = 0x100000000ULL;
    }
    
    serial_printf("Kernel phys ends at: 0x%lx, mapping up to: 0x%lx\n", kernel_phys_end, map_limit);
    
    uint64_t needed_pages = 1 + 8 + 2048 + (map_limit / (2 * 1024 * 1024)) * 2;
    uint64_t needed_mem = needed_pages * PAGE_SIZE;
    
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && 
            entry->length >= needed_mem &&
            entry->base >= 0x100000) {
            next_free_page = align_up(entry->base, PAGE_SIZE);
            paging_pool_start = next_free_page;
            paging_pool_end = next_free_page + needed_mem;
            if (paging_pool_end > entry->base + entry->length) {
                paging_pool_end = entry->base + entry->length;
            }
            break;
        }
    }
    
    if (next_free_page == 0) {
        serial_printf("No memory for paging\n");
        return;
    }
    
    serial_printf("Paging pool: 0x%lx-0x%lx (%lu KB)\n", 
                  next_free_page, paging_pool_end, (paging_pool_end - next_free_page) / 1024);
    
    uint64_t pml4_phys = alloc_physical_page();
    if (pml4_phys == 0) {
        serial_printf("Failed to allocate PML4\n");
        return;
    }
    
    pml4 = (page_table_t *)phys_to_virt(pml4_phys);
    
    copy_page_table_entries(pml4, old_pml4);
    
    serial_printf("Copied existing page tables, now mapping additional memory...\n");
    
    serial_printf("Mapping 0x0-0x%lx identity...\n", map_limit);
    for (uint64_t phys = 0; phys < map_limit; phys += PAGE_SIZE) {
        if ((phys & 0x3FFFFFF) == 0) {
            serial_printf(".");
        }
        if (map_page(phys, phys, PAGE_PRESENT | PAGE_WRITE) != 0) {
            serial_printf("\nOut of page tables at 0x%lx\n", phys);
            break;
        }
    }
    serial_printf("\n");
    
    uint64_t hhdm = hhdm_request.response->offset;
    (void)hhdm;
    (void)hhdm;
    (void)hhdm;
    (void)hhdm;
    serial_printf("Mapping 0x0-0x%lx to HHDM (0x%lx)...\n", map_limit, hhdm);
    for (uint64_t phys = 0; phys < map_limit; phys += PAGE_SIZE) {
        if ((phys & 0x3FFFFFF) == 0) {
            serial_printf(".");
        }
        uint64_t virt = phys + hhdm;
        if (map_page(virt, phys, PAGE_PRESENT | PAGE_WRITE) != 0) {
            serial_printf("\nOut of page tables at 0x%lx\n", virt);
            break;
        }
    }
    serial_printf("\n");
    
    serial_printf("Loading new page tables (PML4=0x%lx)...\n", pml4_phys);
    
    load_cr3(pml4_phys);
    
    serial_printf("Paging initialized successfully\n");
}

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

static void coalesce_free_list(void) {
    free_list_block *cur = free_list_head;
    while (cur && cur->next) {
        uint8_t *cur_end = (uint8_t *)cur + sizeof(free_list_block) + cur->size;
        if (cur_end == (uint8_t *)cur->next) {
            cur->size += sizeof(free_list_block) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void init_allocator() {
    serial_printf("Initializing allocator...\n");
    
    if (!memmap_request.response) {
        serial_printf("Memmap response was null\n");
        return;
    }
    if (!hhdm_request.response) {
        serial_printf("HHDM not available\n");
        return;
    }

    init_paging();
    
    serial_printf("Back from init_paging, setting up allocator blocks...\n");

    struct limine_memmap_response *memmap = memmap_request.response;

    uint64_t total_ram = 0;
    for (size_t i = 0; i < memmap->entry_count; ++i) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) total_ram += entry->length;
    }
    serial_printf("Total RAM: %lu MiB\n", total_ram / (1024 * 1024));

    free_list_head = NULL;
    int blocks_added = 0;

    for (size_t i = 0; i < memmap->entry_count; ++i) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE &&
            entry->length > (1 * 1024 * 1024) &&
            entry->base >= 0x00100000) {

            uint64_t phys_start = (entry->base + ALIGN_SIZE - 1) & ~(uint64_t)(ALIGN_SIZE - 1);
            uint64_t phys_end = entry->base + entry->length;
            
            if (phys_start < paging_pool_end && phys_end > paging_pool_start) {
                if (phys_start < paging_pool_end) {
                    phys_start = paging_pool_end;
                }
            }
            
            if (phys_end <= phys_start + sizeof(free_list_block)) continue;

            uint64_t usable_len = phys_end - phys_start;

            free_list_block *block = (free_list_block *)phys_to_virt(phys_start);
            block->size = usable_len - sizeof(free_list_block);
            block->next = NULL;
            insert_free_block_sorted(block);
            blocks_added++;

            serial_printf("Block %d: 0x%lx (%lu KB)\n",
                          blocks_added, phys_start, block->size / 1024);
        }
    }

    if (blocks_added == 0) {
        serial_printf("No usable blocks found\n");
        return;
    }

    coalesce_free_list();

    serial_printf("Allocator ready: %d blocks\n", blocks_added);
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

            *current = block->next;
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

    insert_free_block_sorted(block);

    coalesce_free_list();

    restore_flags(flags);
}

uint64_t create_user_pml4(void) {
    uint64_t old_cr3 = read_cr3();
    page_table_t *old = (page_table_t *)phys_to_virt(old_cr3 & ~0xFFFULL);

    uint64_t new_pml4_phys = alloc_physical_page();
    if (new_pml4_phys == 0) return 0;

    page_table_t *new_pml4 = (page_table_t *)phys_to_virt(new_pml4_phys);
    // Copy existing entries (kernel mappings)
    for (int i = 0; i < 512; i++) {
        new_pml4->entries[i] = old->entries[i];
    }

    // Ensure lower half user entries are cleared to avoid accidental kernel writes
    for (int i = 0; i < 256; i++) {
        // if present, mark as user if mapping is intended to be user-accessible
        if (new_pml4->entries[i] & PAGE_PRESENT) {
            new_pml4->entries[i] |= PAGE_USER;
        }
    }

    return new_pml4_phys;
}

void load_pml4(uint64_t pml4_phys) {
    load_cr3(pml4_phys);
}

uint64_t allocate_user_stack(uint64_t pml4_phys, uint64_t user_vaddr, size_t size) {
    if (size == 0) return 0;
    size = align_up(size, PAGE_SIZE);

    page_table_t *old_pml4 = pml4;
    page_table_t *new_pml4 = (page_table_t *)phys_to_virt(pml4_phys);
    if (!new_pml4) return 0;

    // Temporarily point global pml4 to new table so map_page operates on it
    pml4 = new_pml4;

    uint64_t stack_base_v = user_vaddr - size;
    uint64_t first_phys = 0;

    for (uint64_t v = stack_base_v; v < user_vaddr; v += PAGE_SIZE) {
        uint64_t phys = alloc_physical_page();
        if (phys == 0) {
            // roll back? simple: leave partial allocation
            break;
        }
        if (first_phys == 0) first_phys = phys;
        if (map_page(v, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER) != 0) {
            // failed to map
            break;
        }
    }

    // restore global pml4
    pml4 = old_pml4;

    return first_phys;
}

int unmap_page(uint64_t vaddr) {
    if (!pml4) return -1;

    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;

    page_table_t *pml4_table = pml4;
    if (!(pml4_table->entries[pml4_idx] & PAGE_PRESENT)) return -1;
    page_table_t *pdpt = (page_table_t *)phys_to_virt(pml4_table->entries[pml4_idx] & ~0xFFFULL);
    if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT)) return -1;
    page_table_t *pd = (page_table_t *)phys_to_virt(pdpt->entries[pdpt_idx] & ~0xFFFULL);
    if (!(pd->entries[pd_idx] & PAGE_PRESENT)) return -1;
    page_table_t *pt = (page_table_t *)phys_to_virt(pd->entries[pd_idx] & ~0xFFFULL);
    if (!(pt->entries[pt_idx] & PAGE_PRESENT)) return -1;

    pt->entries[pt_idx] = 0;
    invlpg(vaddr);
    return 0;
}

void destroy_user_mappings(uint64_t pml4_phys) {
    if (pml4_phys == 0) return;

    page_table_t *target = (page_table_t *)phys_to_virt(pml4_phys);
    if (!target) return;

    for (int pml4_i = 0; pml4_i < 256; pml4_i++) {
        uint64_t pml4_entry = target->entries[pml4_i];
        if (!(pml4_entry & PAGE_PRESENT)) continue;
        page_table_t *pdpt = (page_table_t *)phys_to_virt(pml4_entry & ~0xFFFULL);
        if (!pdpt) continue;
        for (int pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            uint64_t pdpt_entry = pdpt->entries[pdpt_i];
            if (!(pdpt_entry & PAGE_PRESENT)) continue;
            page_table_t *pd = (page_table_t *)phys_to_virt(pdpt_entry & ~0xFFFULL);
            if (!pd) continue;
            for (int pd_i = 0; pd_i < 512; pd_i++) {
                uint64_t pd_entry = pd->entries[pd_i];
                if (!(pd_entry & PAGE_PRESENT)) continue;
                page_table_t *pt = (page_table_t *)phys_to_virt(pd_entry & ~0xFFFULL);
                if (!pt) continue;

                // iterate PTEs and free any mapped physical pages
                for (int pt_i = 0; pt_i < 512; pt_i++) {
                    uint64_t pte = pt->entries[pt_i];
                    if (pte & PAGE_PRESENT) {
                        uint64_t mapped_phys = pte & ~0xFFFULL;
                        uint64_t vaddr = ((uint64_t)pml4_i << 39) | ((uint64_t)pdpt_i << 30) | ((uint64_t)pd_i << 21) | ((uint64_t)pt_i << 12);
                        pt->entries[pt_i] = 0;
                        invlpg(vaddr);
                        free_physical_page(mapped_phys);
                    }
                }

                // free the page table page itself
                uint64_t pt_phys = virt_to_phys(pt);
                free_physical_page(pt_phys);

                // clear the PD entry
                pd->entries[pd_i] = 0;
            }

            // free the PD page
            uint64_t pd_phys = virt_to_phys(pd);
            free_physical_page(pd_phys);

            // clear the PDPT entry
            pdpt->entries[pdpt_i] = 0;
        }

        // free the PDPT page
        uint64_t pdpt_phys = virt_to_phys(pdpt);
        free_physical_page(pdpt_phys);

        // clear the PML4 entry
        target->entries[pml4_i] = 0;
    }

    // free the PML4 page itself
    uint64_t pml4_phys_val = virt_to_phys(target);
    free_physical_page(pml4_phys_val);
}
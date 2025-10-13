#ifndef MEM_H
#define MEM_H

#include <stddef.h>
#include <stdint.h>

typedef struct free_list_block {
    size_t size;
    struct free_list_block *next;
} free_list_block;

void init_allocator(void);
void *kmalloc(size_t size);       
void kfree(void *ptr);
void init_paging(void);
int map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags);
uint64_t create_user_pml4(void);
void load_pml4(uint64_t pml4_phys);
uint64_t allocate_user_stack(uint64_t pml4_phys, uint64_t user_vaddr, size_t size);
int unmap_page(uint64_t vaddr);
void destroy_user_mappings(uint64_t pml4_phys);

#endif
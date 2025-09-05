//#ifndef MEM_H
//#define MEM_H
//
//#include <stdint.h>
//#include <stddef.h>
//
//void *memcpy(void *restrict dest, const void *restrict src, size_t n);
//void *memset(void *s, int c, size_t n);
//void *memmove(void *dest, const void *src, size_t n);
//int memcmp(const void *s1, const void *s2, size_t n);
//
//#define MIN_ORDER 12  // 4 KiB
//#define MAX_ORDER 20  // 1 MiB
//
//void buddy_init(void* base, size_t length);
//void* buddy_alloc(size_t size);
//void buddy_free(void* ptr, size_t size);
//
//#define PAGE_PRESENT   0x001
//#define PAGE_WRITABLE  0x002
//#define PAGE_USER      0x004
//
//void paging_init(void* early_mem_base, size_t early_mem_size);
//void paging_map_page(uintptr_t virt, uintptr_t phys, uint64_t flags);
//
//void* page_alloc(void);
//
//void page_free(void* page);
//
//
//#endif // MEM_H
#include <stdint.h>
#include <limine.h>
#include <memory.h>

__attribute__((used))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

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
//====================Buddy Allocator================================
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

    // Align size down to MAX_ORDER
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

    // Split blocks down
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
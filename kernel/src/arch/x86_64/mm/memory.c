#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include "memory.h"

#define HEAP_SIZE 4096 * 1024

typedef struct block {
    size_t size;
    int is_free;
    struct block* next;
} block_t;

static block_t* heap_start = NULL;
static char heap_memory[HEAP_SIZE];

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

void init_heap() {
    heap_start = (block_t*)heap_memory;
    heap_start->size = HEAP_SIZE - sizeof(block_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
}

static block_t* find_free_block(size_t size) {
    block_t* current = heap_start;
    
    while (current) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

static void split_block(block_t* block, size_t size) {
    if (block->size > size + sizeof(block_t)) {
        block_t* new_block = (block_t*)((char*)block + sizeof(block_t) + size);
        new_block->size = block->size - size - sizeof(block_t);
        new_block->is_free = 1;
        new_block->next = block->next;
        
        block->size = size;
        block->next = new_block;
    }
}

static void coalesce() {
    block_t* current = heap_start;
    
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            current->size += current->next->size + sizeof(block_t);
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    if (!heap_start) {
        init_heap();
    }
    
    block_t* block = find_free_block(size);
    if (!block) {
        return NULL;
    }
    
    split_block(block, size);
    
    block->is_free = 0;
    
    return (char*)block + sizeof(block_t);
}

void free(void* ptr) {
    if (!ptr) return;
    
    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    
    block->is_free = 1;
    
    coalesce();
}

void* calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void* ptr = malloc(total_size);
    
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    block_t* block = (block_t*)((char*)ptr - sizeof(block_t));
    
    if (block->size >= size) {
        return ptr;
    }
    
    void* new_ptr = malloc(size);
    if (new_ptr) {
        size_t copy_size = (block->size < size) ? block->size : size;
        memcpy(new_ptr, ptr, copy_size);
        free(ptr);
    }
    
    return new_ptr;
}

void print_heap_status() {
    block_t* current = heap_start;
    int block_count = 0;
    size_t total_free = 0;
    size_t total_allocated = 0;
    
    while (current) {
        block_count++;
        if (current->is_free) {
            total_free += current->size;
        } else {
            total_allocated += current->size;
        }
        current = current->next;
    }
}

void get_heap_stats(size_t* total_size, size_t* used_size, size_t* free_size) {
    if (!heap_start) {
        *total_size = *used_size = *free_size = 0;
        return;
    }
    
    *total_size = HEAP_SIZE;
    *used_size = 0;
    *free_size = 0;
    
    block_t* current = heap_start;
    while (current) {
        if (current->is_free) {
            *free_size += current->size;
        } else {
            *used_size += current->size;
        }
        current = current->next;
    }
}

// Memory utility functions (your existing ones are good)
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


int strcmp(const char *str1, const char *str2) {
    if (!str1 || !str2) return str1 ? 1 : (str2 ? -1 : 0);
    
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return (unsigned char)*str1 - (unsigned char)*str2;
}

int strlen(const char *str) {
    if (!str) return 0;
    
    int len = 0;
    while (str[len]) len++;
    return len;
}

char *strncpy(char *dest, const char *src, int n) {
    if (!dest || !src) return dest;
    
    int i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char *strcpy(char *dest, const char *src) {
    if (!dest || !src) return dest;
    
    int i = 0;
    while (src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
    return dest;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    if (!s1 || !s2) return s1 ? 1 : (s2 ? -1 : 0);
    
    while (n && *s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}
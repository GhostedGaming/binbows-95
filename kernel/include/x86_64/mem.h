#ifndef MEM_H
#define MEM_H
#include <stddef.h>

typedef struct free_list_block {
    size_t size;
    struct free_list_block *next;
} free_list_block;

void init_allocator();
void *kmalloc(size_t size);       
void kfree(void *ptr);         

#endif
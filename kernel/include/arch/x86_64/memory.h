#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

void init_heap(void);
void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);

void* memcpy(void* restrict dest, const void* restrict src, size_t n);
void* memset(void* s, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
int strcmp(const char *str1, const char *str2);
int strlen(const char *str);
char *strncpy(char *dest, const char *src, int n);
char *strcpy(char *dest, const char *src);

void print_heap_status(void);
void get_heap_stats(size_t* total_size, size_t* used_size, size_t* free_size);

#endif // MEMORY_H
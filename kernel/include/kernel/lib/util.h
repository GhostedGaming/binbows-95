#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void* memcpy(void *restrict dest, const void *restrict src, size_t n);
void* memset(void *s, int c, size_t n);

#endif
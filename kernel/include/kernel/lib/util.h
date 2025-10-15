#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

// Memory functions
void* memcpy(void *restrict dest, const void *restrict src, size_t n);
void* memset(void *s, int c, size_t n);
void* memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

// String functions
size_t strlen(const char *str);
char* strchr(const char *s, int c);
int strcmp(const char *str1, const char *str2);
char* strcpy(char *dest, const char *src);
char* strncpy(char *dest, const char *src, int n);
char* strcat(char* dest, const char* src);
char* strtok(char* str, const char* delim);
char* strrchr(const char* str, int c);
char* str_append(const char* str1, const char* str2);

// Character conversion and classification
int char_to_int(char c);
int atoi(const char *str);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isspace(int c);
int toupper(int c);
int tolower(int c);

// Formatting
int vsnprintf(char *buf, size_t size, const char *format, va_list args);

// Hashing
uint32_t hash_string(const char *str);

#endif
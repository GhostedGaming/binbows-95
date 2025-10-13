#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

int strlen(const char *str);
char* strchr(const char *s, int c);
int strcmp(const char *str1, const char *str2);
char* strcpy(char *dest, const char *src);
char* strncpy(char *dest, const char *src, int n);
int char_to_int(char c);
char* str_append(const char* str1, const char* str2);
char* strcat(char* dest, const char* src);
int atoi(const char *str);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isspace(int c);
int toupper(int c);
int tolower(int c);
uint32_t hash_string(const char *str);
int vsnprintf(char *buf, size_t size, const char *format, va_list args);
void* memcpy(void *restrict dest, const void *restrict src, size_t n);
void* memset(void *s, int c, size_t n);
void* memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

#endif
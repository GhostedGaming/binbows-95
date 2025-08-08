#ifndef UTIL_H
#define UTIL_H

int strlen(const char *str);
char *strchr(const char *s, int c);
int strcmp(const char *str1, const char *str2);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int n);
int char_to_int(char c);
char* str_append(const char* str1, const char* str2);
char* strcat(char* dest, const char* src);

#endif // UTIL_H
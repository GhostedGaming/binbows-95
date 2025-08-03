#include <util.h>
#include <stddef.h>

int strlen(const char *str) {
    int len = 0;
    while (str && *str++) len++;
    return len;
}

char *strchr(const char *s, int c) {
    while (*s != '\0') {
        if (*s == c) {
            return (char *)s;
        }
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}
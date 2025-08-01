#include <util.h>

int strlen(const char *str) {
    int len = 0;
    while (str && *str++) len++;
    return len;
}
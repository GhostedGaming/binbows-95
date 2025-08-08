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

int strcmp(const char *str1, const char *str2) {
    if (!str1 || !str2) return str1 ? 1 : (str2 ? -1 : 0);
    
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return (unsigned char)*str1 - (unsigned char)*str2;
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

int char_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    return -1;
}

char* strcat(char* dest, const char* src) {
    if (!dest || !src) {
        return dest;
    }
    
    char* original_dest = dest;
    
    while (*dest) {
        dest++;
    }

    while (*src) {
        *dest = *src;
        dest++;
        src++;
    }
    
    *dest = '\0';
    
    return original_dest;
}

char* str_append(const char* str1, const char* str2) {
    static char buffer[1024];
    
    if (str1 == NULL || str2 == NULL) {
        return NULL;
    }
    
    strcpy(buffer, str1);
    strcat(buffer, str2);
    
    return buffer;
}

int atoi(const char *str) {
    if (!str) return 0;
    
    int result = 0;
    int sign = 1;
    int i = 0;
    
    while (str[i] == ' ' || str[i] == '\t') i++;
    
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }
    
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }
    
    return result * sign;
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isalpha(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}
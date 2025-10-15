#include <util.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/*
size_t strlen(const char *str) {
    size_t len = 0;
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
    return dest;
}

// Custom strtok implementation
static char* strtok_last = NULL;
char* strtok(char* str, const char* delim) {
    char* token;
    if (str == NULL) {
        str = strtok_last;
    }
    if (str == NULL) {
        return NULL;
    }

    // Skip leading delimiters
    while (*str != '\0' && strchr(delim, *str) != NULL) {
        str++;
    }
    if (*str == '\0') {
        strtok_last = NULL;
        return NULL;
    }

    token = str;
    while (*str != '\0' && strchr(delim, *str) == NULL) {
        str++;
    }

    if (*str != '\0') {
        *str = '\0';
        str++;
    }
    strtok_last = str;
    return token;
}

// Custom strrchr implementation
char* strrchr(const char* str, int c) {
    const char* last = NULL;
    while (*str != '\0') {
        if (*str == (char)c) {
            last = str;
        }
        str++;
    }
    if (c == '\0') return (char*)str; // Handle null terminator case
    return (char*)last;
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

uint32_t hash_string(const char *str) {
    uint32_t hash = 0x811c9dc5;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 0x01000193;
    }
    return hash;
}

static void reverse_string(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static int ulonglong_to_str(unsigned long long num, char *str, int base) {
    int i = 0;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    while (num != 0) {
        unsigned long long rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    
    return i;
}

static int ulong_to_str(unsigned long num, char *str, int base) {
    int i = 0;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    while (num != 0) {
        unsigned long rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    
    return i;
}

static int int_to_str(int num, char *str, int base) {
    int i = 0;
    int is_negative = 0;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    if (num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }
    
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    
    return i;
}

static int uint_to_str(unsigned int num, char *str, int base) {
    int i = 0;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    while (num != 0) {
        unsigned int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    
    return i;
}

int vsnprintf(char *buf, size_t size, const char *format, va_list args) {
    size_t pos = 0;
    
    if (!buf || !format || size == 0) {
        return 0;
    }
    
    while (*format && pos < size - 1) {
        if (*format == '%') {
            format++;
            
            if (*format == '\0') break;
            
            // Check for length modifiers 'l' and 'll'
            int is_long = 0;
            int is_longlong = 0;
            if (*format == 'l') {
                is_long = 1;
                format++;
                if (*format == '\0') break;
                if (*format == 'l') {
                    is_longlong = 1;
                    is_long = 0;
                    format++;
                    if (*format == '\0') break;
                }
            }
            
            if (*format == '%') {
                buf[pos++] = '%';
            } else if (*format == 's') {
                char *s = va_arg(args, char*);
                if (s == NULL) s = "(null)";
                while (*s && pos < size - 1) {
                    buf[pos++] = *s++;
                }
            } else if (*format == 'c') {
                buf[pos++] = (char)va_arg(args, int);
            } else if (*format == 'd' || *format == 'i') {
                if (is_long) {
                    long val = va_arg(args, long);
                    char temp[32];
                    int len = int_to_str((int)val, temp, 10);
                    for (int i = 0; i < len && pos < size - 1; i++) {
                        buf[pos++] = temp[i];
                    }
                } else {
                    int val = va_arg(args, int);
                    char temp[32];
                    int len = int_to_str(val, temp, 10);
                    for (int i = 0; i < len && pos < size - 1; i++) {
                        buf[pos++] = temp[i];
                    }
                }
            } else if (*format == 'u') {
                if (is_longlong) {
                    unsigned long long val = va_arg(args, unsigned long long);
                    char temp[32];
                    int len = ulonglong_to_str(val, temp, 10);
                    for (int i = 0; i < len && pos < size - 1; i++) {
                        buf[pos++] = temp[i];
                    }
                } else if (is_long) {
                    unsigned long val = va_arg(args, unsigned long);
                    char temp[32];
                    int len = ulong_to_str(val, temp, 10);
                    for (int i = 0; i < len && pos < size - 1; i++) {
                        buf[pos++] = temp[i];
                    }
                } else {
                    unsigned int val = va_arg(args, unsigned int);
                    char temp[32];
                    int len = uint_to_str(val, temp, 10);
                    for (int i = 0; i < len && pos < size - 1; i++) {
                        buf[pos++] = temp[i];
                    }
                }
            } else if (*format == 'x') {
                if (is_longlong) {
                    unsigned long long val = va_arg(args, unsigned long long);
                    char temp[32];
                    int len = ulonglong_to_str(val, temp, 16);
                    for (int i = 0; i < len && pos < size - 1; i++) {
                        buf[pos++] = temp[i];
                    }
                } else if (is_long) {
                    unsigned long val = va_arg(args, unsigned long);
                    char temp[32];
                    int len = ulong_to_str(val, temp, 16);
                    for (int i = 0; i < len && pos < size - 1; i++) {
                        buf[pos++] = temp[i];
                    }
                } else {
                    unsigned int val = va_arg(args, unsigned int);
                    char temp[32];
                    int len = uint_to_str(val, temp, 16);
                    for (int i = 0; i < len && pos < size - 1; i++) {
                        buf[pos++] = temp[i];
                    }
                }
            } else if (*format == 'p') {
                buf[pos++] = '0';
                buf[pos++] = 'x';
                unsigned long val = (unsigned long)va_arg(args, void*);
                char temp[32];
                int len = ulong_to_str(val, temp, 16);
                for (int i = 0; i < len && pos < size - 1; i++) {
                    buf[pos++] = temp[i];
                }
            }
            format++;
        } else {
            buf[pos++] = *format++;
        }
    }
    
    buf[pos] = '\0';
    return (int)pos;
}

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

/*
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
*/
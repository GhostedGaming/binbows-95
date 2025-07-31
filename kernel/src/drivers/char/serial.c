#include <io.h>
#include <serial.h>
#include <stdarg.h>
#include <stdint.h>

#define PORT 0x3F8 // COM1

int init_serial(void) {
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x80);
    outb(PORT + 0, 0x03);
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x03);
    outb(PORT + 2, 0xC7);
    outb(PORT + 4, 0x0B);
    outb(PORT + 4, 0x1E);
    outb(PORT + 0, 0xAE);
    if (inb(PORT + 0) != 0xAE)
        return 1;
    outb(PORT + 4, 0x0F);
    return 0;
}

static int is_transmit_empty(void) {
    return inb(PORT + 5) & 0x20;
}

static void write_serial_char(char a) {
    while (!is_transmit_empty());
    outb(PORT, a);
}

void write_serial(const char *str) {
    while (*str) write_serial_char(*str++);
}

// Helper: print string with padding
static void write_padded(const char *str, int width, char pad) {
    int len = 0;
    const char *s = str;
    while (*s++) len++;
    while (len < width--) write_serial_char(pad);
    write_serial(str);
}

static void utoa(unsigned long val, char *buf, int base) {
    char *ptr = buf, *ptr1 = buf, tmp;
    do {
        *ptr++ = "0123456789abcdef"[val % base];
        val /= base;
    } while (val);
    *ptr-- = '\0';
    while (ptr1 < ptr) {
        tmp = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp;
    }
}

static void itoa(long val, char *buf, int base) {
    if (val < 0 && base == 10) {
        *buf++ = '-';
        utoa((unsigned long)(-val), buf, base);
    } else {
        utoa((unsigned long)val, buf, base);
    }
}

void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[64];

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            char pad_char = ' ';
            int width = 0;
            int long_flag = 0;

            // Parse flags
            if (*fmt == '0') {
                pad_char = '0';
                fmt++;
            }

            // Parse width
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt++ - '0');
            }

            // Check for long modifier
            if (*fmt == 'l') {
                long_flag = 1;
                fmt++;
            }

            // Conversion specifier
            switch (*fmt) {
                case 'd':
                case 'i': {
                    if (long_flag) {
                        long val = va_arg(args, long);
                        itoa(val, buffer, 10);
                    } else {
                        int val = va_arg(args, int);
                        itoa(val, buffer, 10);
                    }
                    write_padded(buffer, width, pad_char);
                    break;
                }
                case 'u': {
                    if (long_flag) {
                        unsigned long val = va_arg(args, unsigned long);
                        utoa(val, buffer, 10);
                    } else {
                        unsigned int val = va_arg(args, unsigned int);
                        utoa(val, buffer, 10);
                    }
                    write_padded(buffer, width, pad_char);
                    break;
                }
                case 'x':
                case 'X': {
                    if (long_flag) {
                        unsigned long val = va_arg(args, unsigned long);
                        utoa(val, buffer, 16);
                    } else {
                        unsigned int val = va_arg(args, unsigned int);
                        utoa(val, buffer, 16);
                    }
                    if (*fmt == 'X') {
                        for (char *p = buffer; *p; p++) {
                            if (*p >= 'a' && *p <= 'f')
                                *p = *p - 'a' + 'A';
                        }
                    }
                    write_padded(buffer, width, pad_char);
                    break;
                }
                case 'c': {
                    char ch = (char)va_arg(args, int);
                    write_serial_char(ch);
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char *);
                    write_serial(str ? str : "(null)");
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void *);
                    write_serial("0x");
                    utoa((uintptr_t)ptr, buffer, 16);
                    write_padded(buffer, width ? width : 16, '0');
                    break;
                }
                case '%': {
                    write_serial_char('%');
                    break;
                }
                default:
                    write_serial_char('%');
                    write_serial_char(*fmt);
                    break;
            }
            fmt++;
        } else {
            write_serial_char(*fmt++);
        }
    }

    va_end(args);
}

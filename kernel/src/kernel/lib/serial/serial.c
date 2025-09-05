#include <io.h>
#include <serial.h>

#define PORT 0x3F8 // COM1

int init_serial(void) {
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x80);
    outb(PORT + 0, 0x03); // Baud rate divisor (low byte)
    outb(PORT + 1, 0x00); // Baud rate divisor (high byte)
    outb(PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7); // Enable FIFO, clear them, 14-byte threshold
    outb(PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
    outb(PORT + 4, 0x1E); // Set in loopback mode for testing
    outb(PORT + 0, 0xAE); // Test byte
    if (inb(PORT + 0) != 0xAE) return 1;
    outb(PORT + 4, 0x0F); // Back to normal operation mode
    return 0;
}

static int is_transmit_empty(void) {
    return inb(PORT + 5) & 0x20;
}

void write_serial_char(char a) {
    while (!is_transmit_empty());
    outb(PORT, a);
}

void write_serial(const char *str) {
    while (*str) write_serial_char(*str++);
}

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

            if (*fmt == '0') {
                pad_char = '0';
                fmt++;
            }

            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt++ - '0');
            }

            if (*fmt == 'l') {
                long_flag = 1;
                fmt++;
            }

            switch (*fmt) {
                case 'd':
                case 'i': {
                    long val = long_flag ? va_arg(args, long) : va_arg(args, int);
                    itoa(val, buffer, 10);
                    write_padded(buffer, width, pad_char);
                    break;
                }
                case 'u': {
                    unsigned long val = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
                    utoa(val, buffer, 10);
                    write_padded(buffer, width, pad_char);
                    break;
                }
                case 'x':
                case 'X': {
                    unsigned long val = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
                    utoa(val, buffer, 16);
                    if (*fmt == 'X') {
                        for (char *p = buffer; *p; p++) {
                            if (*p >= 'a' && *p <= 'f') *p = *p - 'a' + 'A';
                        }
                    }
                    write_padded(buffer, width, pad_char);
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void *);
                    write_serial("0x");
                    utoa((uintptr_t)ptr, buffer, 16);
                    write_padded(buffer, width ? width : 16, '0');
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
                case '%': {
                    write_serial_char('%');
                    break;
                }
                default: {
                    write_serial_char('%');
                    write_serial_char(*fmt);
                    break;
                }
            }
            fmt++;
        } else {
            write_serial_char(*fmt++);
        }
    }

    va_end(args);
}

int kvsnprintf(char *buffer, size_t size, const char *fmt, va_list args) {
    char *ptr = buffer;
    char *end = buffer + size - 1;
    char temp[32];
    
    while (*fmt && ptr < end) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd': {
                    int val = va_arg(args, int);
                    char *t = temp;
                    int neg = 0;
                    
                    if (val < 0) {
                        neg = 1;
                        val = -val;
                    }
                    
                    // Convert to string
                    if (val == 0) {
                        *t++ = '0';
                    } else {
                        while (val > 0) {
                            *t++ = '0' + (val % 10);
                            val /= 10;
                        }
                    }
                    
                    if (neg && ptr < end) *ptr++ = '-';
                    
                    // Reverse and copy
                    while (t > temp && ptr < end) {
                        *ptr++ = *--t;
                    }
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    char *t = temp;
                    const char *hex = "0123456789abcdef";
                    
                    if (val == 0) {
                        *t++ = '0';
                    } else {
                        while (val > 0) {
                            *t++ = hex[val % 16];
                            val /= 16;
                        }
                    }
                    
                    while (t > temp && ptr < end) {
                        *ptr++ = *--t;
                    }
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char *);
                    if (!str) str = "(null)";
                    while (*str && ptr < end) {
                        *ptr++ = *str++;
                    }
                    break;
                }
                case 'c': {
                    char ch = (char)va_arg(args, int);
                    if (ptr < end) *ptr++ = ch;
                    break;
                }
                case '%': {
                    if (ptr < end) *ptr++ = '%';
                    break;
                }
                default: {
                    if (ptr < end) *ptr++ = '%';
                    if (ptr < end) *ptr++ = *fmt;
                    break;
                }
            }
            fmt++;
        } else {
            *ptr++ = *fmt++;
        }
    }
    
    *ptr = '\0';
    return ptr - buffer;
}
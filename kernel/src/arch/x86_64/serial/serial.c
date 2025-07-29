#include <io.h>
#include <serial.h>
#include <stdarg.h>
#include <stdint.h>

#define PORT 0x3F8 // COM1

int init_serial(void) {
    outb(PORT + 1, 0x00); // Disable all interrupts
    outb(PORT + 3, 0x80); // Enable DLAB
    outb(PORT + 0, 0x03); // Set divisor to 3 (lo byte)
    outb(PORT + 1, 0x00); // (hi byte)
    outb(PORT + 3, 0x03); // 8N1
    outb(PORT + 2, 0xC7); // Enable FIFO, clear them
    outb(PORT + 4, 0x0B); // IRQs enabled
    outb(PORT + 4, 0x1E); // Loopback test
    outb(PORT + 0, 0xAE); // Send test byte

    if (inb(PORT + 0) != 0xAE) {
        return 1; // Failed
    }

    outb(PORT + 4, 0x0F); // Normal mode
    return 0;
}

static int is_transmit_empty(void) {
    return inb(PORT + 5) & 0x20;
}

static void write_serial_char(char a) {
    while (is_transmit_empty() == 0);
    outb(PORT, a);
}

void write_serial(const char *str) {
    while (*str) {
        write_serial_char(*str++);
    }
    // Automatically append \r\n
    write_serial_char('\r');
    write_serial_char('\n');
}

// Helper function to convert integer to string
static void int_to_string(int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int tmp_value;

    // Handle negative numbers for decimal base
    if (value < 0 && base == 10) {
        *ptr++ = '-';
        value = -value;
        ptr1++;
    }

    // Convert to string (reversed)
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    // Reverse string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

// Helper function to convert unsigned integer to string
static void uint_to_string(unsigned int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    unsigned int tmp_value;

    // Convert to string (reversed)
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    // Reverse string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void serial_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[32]; // Buffer for number conversions
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd':
                case 'i': {
                    int value = va_arg(args, int);
                    int_to_string(value, buffer, 10);
                    write_serial(buffer);
                    break;
                }
                case 'u': {
                    unsigned int value = va_arg(args, unsigned int);
                    uint_to_string(value, buffer, 10);
                    write_serial(buffer);
                    break;
                }
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    uint_to_string(value, buffer, 16);
                    write_serial(buffer);
                    break;
                }
                case 'X': {
                    unsigned int value = va_arg(args, unsigned int);
                    uint_to_string(value, buffer, 16);
                    // Convert to uppercase
                    for (int i = 0; buffer[i]; i++) {
                        if (buffer[i] >= 'a' && buffer[i] <= 'f') {
                            buffer[i] = buffer[i] - 'a' + 'A';
                        }
                    }
                    write_serial(buffer);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    write_serial_char(c);
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char*);
                    if (str) {
                        write_serial(str);
                    } else {
                        write_serial("(null)");
                    }
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void*);
                    write_serial("0x");
                    uint_to_string((uintptr_t)ptr, buffer, 16);
                    write_serial(buffer);
                    break;
                }
                case '%': {
                    write_serial_char('%');
                    break;
                }
                default: {
                    write_serial_char('%');
                    write_serial_char(*format);
                    break;
                }
            }
        } else {
            write_serial_char(*format);
        }
        format++;
    }
    
    va_end(args);
} 
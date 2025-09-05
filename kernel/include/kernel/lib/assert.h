#ifndef ASSERT_H
#define ASSERT_H
#include <serial.h>

#define ASSERT(expr) if (!(expr)) { \
    serial_printf("Assertion failed: " #expr "\n"); \
    serial_printf("File: " __FILE__ " Line: %d", __LINE__); \
    for (;;) asm volatile ("hlt"); \
}

#endif
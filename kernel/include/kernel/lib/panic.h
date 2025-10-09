#ifndef PANIC_H
#define PANIC_H

#include <serial.h>

void panic_impl(const char *file, int line, const char *func, const char *fmt, ...);
void print_registers(void);
void print_stack_trace(void);

#define panic(fmt, ...) panic_impl(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define assert(expr) \
    do { \
        if (!(expr)) { \
            panic("Assertion failed: %s", #expr); \
        } \
    } while (0)

#define kwarn(fmt, ...) \
    do { \
        serial_printf("WARNING at %s:%d: ", __FILE__, __LINE__); \
        serial_printf(fmt, ##__VA_ARGS__); \
        serial_printf("\n"); \
    } while (0)

#endif
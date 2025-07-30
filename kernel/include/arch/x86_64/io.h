#ifndef IO_H
#define IO_H

#include <stdint.h>

// 8-bit port I/O
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

// 16-bit port I/O
static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %0, %1" : : "a"(data), "Nd"(port));
}

// 32-bit port I/O
static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    asm volatile ("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outl(uint16_t port, uint32_t data) {
    asm volatile ("outl %0, %1" : : "a"(data), "Nd"(port));
}

// I/O delay functions
static inline void io_wait(void) {
    outb(0x80, 0);
}

// Alternative I/O wait using unused port
static inline void io_delay(void) {
    asm volatile ("jmp 1f\n\t"
                  "1:jmp 2f\n\t"
                  "2:" ::: "memory");
}

// String I/O operations (for bulk data transfer)
static inline void insb(uint16_t port, void *addr, uint32_t count) {
    asm volatile ("rep insb" 
                  : "+D"(addr), "+c"(count) 
                  : "d"(port) 
                  : "memory");
}

static inline void outsb(uint16_t port, const void *addr, uint32_t count) {
    asm volatile ("rep outsb" 
                  : "+S"(addr), "+c"(count) 
                  : "d"(port));
}

static inline void insw(uint16_t port, void *addr, uint32_t count) {
    asm volatile ("rep insw" 
                  : "+D"(addr), "+c"(count) 
                  : "d"(port) 
                  : "memory");
}

static inline void outsw(uint16_t port, const void *addr, uint32_t count) {
    asm volatile ("rep outsw" 
                  : "+S"(addr), "+c"(count) 
                  : "d"(port));
}

static inline void insl(uint16_t port, void *addr, uint32_t count) {
    asm volatile ("rep insl" 
                  : "+D"(addr), "+c"(count) 
                  : "d"(port) 
                  : "memory");
}

static inline void outsl(uint16_t port, const void *addr, uint32_t count) {
    asm volatile ("rep outsl" 
                  : "+S"(addr), "+c"(count) 
                  : "d"(port));
}

// Memory-mapped I/O helpers
static inline uint8_t mmio_read8(volatile void *addr) {
    return *(volatile uint8_t*)addr;
}

static inline void mmio_write8(volatile void *addr, uint8_t value) {
    *(volatile uint8_t*)addr = value;
}

static inline uint16_t mmio_read16(volatile void *addr) {
    return *(volatile uint16_t*)addr;
}

static inline void mmio_write16(volatile void *addr, uint16_t value) {
    *(volatile uint16_t*)addr = value;
}

static inline uint32_t mmio_read32(volatile void *addr) {
    return *(volatile uint32_t*)addr;
}

static inline void mmio_write32(volatile void *addr, uint32_t value) {
    *(volatile uint32_t*)addr = value;
}

static inline uint64_t mmio_read64(volatile void *addr) {
    return *(volatile uint64_t*)addr;
}

static inline void mmio_write64(volatile void *addr, uint64_t value) {
    *(volatile uint64_t*)addr = value;
}

// Memory barriers for ordering
static inline void memory_barrier(void) {
    asm volatile ("mfence" ::: "memory");
}

static inline void read_barrier(void) {
    asm volatile ("lfence" ::: "memory");
}

static inline void write_barrier(void) {
    asm volatile ("sfence" ::: "memory");
}

// Compiler barrier (prevents reordering by compiler)
static inline void compiler_barrier(void) {
    asm volatile ("" ::: "memory");
}

#endif
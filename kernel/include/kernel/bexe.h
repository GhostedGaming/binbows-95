#ifndef BEXE_H
#define BEXE_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------
 * BEXE File Format
 * ------------------------------
 * Magic: "BEXE" (0x42455845) - 4 bytes little-endian
 * Code Size: uint16 - 2 bytes
 * Data Size: uint16 - 2 bytes
 * BSS Size: uint16 - 2 bytes
 * Total header: 10 bytes
 * 
 * Layout:
 * [0-3]   Magic (0x45 0x58 0x45 0x42 in file)
 * [4-5]   Code section size
 * [6-7]   Data section size
 * [8-9]   BSS section size
 * [10+]   Code section
 * [...]   Data section
 */

typedef struct {
    uint32_t magic;       // 0x42455845 ("BEXE")
    uint16_t code_size;   // Size of code section
    uint16_t data_size;   // Size of data section
    uint16_t bss_size;    // Size of BSS section (uninitialized data)
} __attribute__((packed)) bexe_header_t;

/* ------------------------------
 * CPU Flags
 * ------------------------------
 */
#define FLAG_ZERO     0x01  // Zero flag
#define FLAG_SIGN     0x02  // Sign flag
#define FLAG_LESS     0x04  // Less than
#define FLAG_GREATER  0x08  // Greater than
#define FLAG_CARRY    0x10  // Carry flag
#define FLAG_OVERFLOW 0x20  // Overflow flag

/* ------------------------------
 * Virtual Machine State
 * ------------------------------
 */
typedef struct {
    // Memory
    uint8_t *memory;          // Unified memory space
    uint32_t memory_size;     // Total memory size
    uint32_t code_size;       // Size of code section
    uint32_t data_size;       // Size of data section
    uint32_t bss_size;        // Size of BSS section
    
    // Registers
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsp, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    
    // Control
    uint32_t ip;              // Instruction pointer
    uint8_t flags;            // CPU flags
    bool running;             // Is VM running?
    uint32_t instruction_count;
} vm_t;

/* ------------------------------
 * VM Functions
 * ------------------------------
 */

/**
 * Load a BEXE file from disk into memory
 * @param drive FAT12 drive number
 * @param filename File to load (8.3 format)
 * @return VM instance pointer, or NULL on error
 */
void* bexe_load(uint8_t drive, const char* filename);

/**
 * Execute a loaded BEXE program
 * @param vm_ptr VM instance from bexe_load()
 */
void bexe_execute(void* vm_ptr);

/**
 * Free VM memory
 * @param vm_ptr VM instance to free
 */
void bexe_free(void* vm_ptr);

/**
 * Print VM state (registers, flags, etc.)
 * @param vm VM instance
 */
void bexe_print_state(vm_t* vm);

#endif // BEXE_H
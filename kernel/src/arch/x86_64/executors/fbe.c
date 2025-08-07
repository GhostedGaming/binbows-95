#include <stdint.h>
#include <stddef.h>
#include <mem.h>
#include <ide.h>
#include <fat12.h>
#include <serial.h>
#include <fbe.h>

extern void* global_exec_region;
extern size_t global_exec_size;

void run_bin(const char* name) {
    size_t actual_size = 0;
    
    if (!global_exec_region) {
        serial_printf("Error: No execution region allocated!\n");
        return;
    }
    
    serial_printf("Attempting to load binary: %s\n", name);
    
    if (!fat12_read_file(0, name, global_exec_region, &actual_size)) {
        serial_printf("Failed to read %s\n", name);
        return;
    }
    
    if (actual_size == 0) {
        serial_printf("File size is 0\n");
        return;
    }
    
    if (actual_size > global_exec_size) {
        serial_printf("Binary too large (%zu bytes) for execution region (%zu bytes)\n", 
                     actual_size, global_exec_size);
        return;
    }
    
    serial_printf("Executing %s (%zu bytes) at 0x%p...\n", name, actual_size, global_exec_region);
    
    uint8_t *code = (uint8_t*)global_exec_region;
    serial_printf("First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
        code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7]);
    
    void (*entry_point)(void) = (void (*)(void))global_exec_region;
    serial_printf("Jumping to entry point...\n");
    entry_point();
    serial_printf("Binary returned successfully!\n");
}
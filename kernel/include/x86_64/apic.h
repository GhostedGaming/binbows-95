#ifndef APIC_H
#define APIC_H

#include <stdint.h>

// MADT entry types
#define APIC_TYPE_LOCAL_APIC 0
#define APIC_TYPE_IO_APIC 1
#define APIC_TYPE_INTERRUPT_OVERRIDE 2

// MADT header
typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) apic_header_t;

// Local APIC entry
typedef struct {
    apic_header_t header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) apic_local_apic_t;

// I/O APIC entry
typedef struct {
    apic_header_t header;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_address;
    uint32_t global_system_interrupt_base;
} __attribute__((packed)) apic_io_apic_t;

// Interrupt Source Override entry
typedef struct {
    apic_header_t header;
    uint8_t bus;
    uint8_t source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} __attribute__((packed)) apic_interrupt_override_t;

void apic_init(void);

#endif // APIC_H

#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

// Standard ACPI table header
typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

void acpi_init(void);
void* acpi_find_table(const char* signature);

#endif // ACPI_H

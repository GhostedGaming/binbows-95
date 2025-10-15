#include <acpi.h>
#include <limine.h>
#include <mem.h>
#include <serial.h>
#include <util.h>
#include <stdbool.h>

// Request the ACPI RSDP address from the bootloader
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

static acpi_sdt_header_t* xsdt = NULL;

// Function to verify the checksum of an ACPI table
static bool validate_checksum(acpi_sdt_header_t* header) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < header->length; i++) {
        sum += ((uint8_t*)header)[i];
    }
    return sum == 0;
}

void* acpi_find_table(const char* signature) {
    if (!xsdt) {
        return NULL;
    }

    int entries = (xsdt->length - sizeof(acpi_sdt_header_t)) / 8;
    uint64_t* other_tables = (uint64_t*)((uint8_t*)xsdt + sizeof(acpi_sdt_header_t));

    for (int i = 0; i < entries; i++) {
        acpi_sdt_header_t* header = (acpi_sdt_header_t*)phys_to_virt(other_tables[i]);
        if (memcmp(header->signature, signature, 4) == 0) {
            if (validate_checksum(header)) {
                return header;
            } else {
                serial_printf("ACPI: Found table '%.4s' but checksum is invalid.\n", signature);
                return NULL;
            }
        }
    }

    serial_printf("ACPI: Could not find table '%.4s'\n", signature);
    return NULL;
}

void acpi_init(void) {
    struct limine_rsdp_response* rsdp_response = rsdp_request.response;
    if (!rsdp_response || !rsdp_response->address) {
        serial_printf("ACPI: RSDP not found.\n");
        return;
    }

    // We need the virtual address of the RSDP
    void* rsdp_virt = phys_to_virt(rsdp_response->address);
    acpi_sdt_header_t* rsdt_header = (acpi_sdt_header_t*)phys_to_virt(*(uint32_t*)((uint8_t*)rsdp_virt + 16));

    // Check for XSDT (64-bit pointers)
    xsdt = (acpi_sdt_header_t*)phys_to_virt(*(uint64_t*)((uint8_t*)rsdp_virt + 24));

    if (!xsdt) {
        serial_printf("ACPI: XSDT not found, falling back to RSDT.\n");
        xsdt = rsdt_header;
    }

    if (!validate_checksum(xsdt)) {
        serial_printf("ACPI: XSDT checksum is invalid.\n");
        xsdt = NULL;
        return;
    }

    serial_printf("ACPI: XSDT found at %p and verified.\n", xsdt);
}

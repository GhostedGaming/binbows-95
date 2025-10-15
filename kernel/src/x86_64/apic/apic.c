#include <apic.h>
#include <acpi.h>
#include <mem.h>
#include <serial.h>
#include <util.h>

static volatile uint32_t* lapic_addr = NULL;
static uint32_t io_apic_addr = 0;

// LAPIC Registers
#define LAPIC_REG_ID 0x20
#define LAPIC_REG_SPURIOUS 0xF0
#define LAPIC_REG_EOI 0xB0

static void lapic_write(uint32_t reg, uint32_t value) {
    lapic_addr[reg / 4] = value;
}

static uint32_t lapic_read(uint32_t reg) {
    return lapic_addr[reg / 4];
}

void lapic_eoi() {
    lapic_write(LAPIC_REG_EOI, 0);
}

void apic_init(void) {
    acpi_sdt_header_t* madt = (acpi_sdt_header_t*)acpi_find_table("APIC");
    if (!madt) {
        serial_printf("APIC: MADT not found! Cannot initialize APIC.\n");
        return;
    }

    uint32_t lapic_base_addr = *(uint32_t*)((uint8_t*)madt + 36);
    lapic_addr = (uint32_t*)phys_to_virt(lapic_base_addr);

    serial_printf("APIC: Local APIC physical address: 0x%x\n", lapic_base_addr);
    serial_printf("APIC: Local APIC virtual address: %p\n", lapic_addr);

    // Parse MADT entries
    uint8_t* p = (uint8_t*)madt + sizeof(acpi_sdt_header_t) + 8; // Skip header and local APIC address and flags
    uint8_t* end = (uint8_t*)madt + madt->length;

    while (p < end) {
        apic_header_t* header = (apic_header_t*)p;
        switch (header->type) {
            case APIC_TYPE_IO_APIC: {
                apic_io_apic_t* io_apic = (apic_io_apic_t*)p;
                io_apic_addr = io_apic->io_apic_address;
                serial_printf("APIC: Found I/O APIC at 0x%x\n", io_apic_addr);
                break;
            }
            case APIC_TYPE_INTERRUPT_OVERRIDE: {
                apic_interrupt_override_t* override = (apic_interrupt_override_t*)p;
                serial_printf("APIC: Interrupt override: IRQ %d -> GSI %d\n", override->source, override->global_system_interrupt);
                break;
            }
        }
        p += header->length;
    }

    // Enable the Local APIC by setting the Spurious Interrupt Vector register
    // Set bit 8 to enable the APIC
    uint32_t spurious_reg = lapic_read(LAPIC_REG_SPURIOUS);
    spurious_reg |= (1 << 8); // Enable APIC
    spurious_reg |= 0xFF;     // Set spurious vector to 255
    lapic_write(LAPIC_REG_SPURIOUS, spurious_reg);

    serial_printf("APIC: Local APIC enabled with spurious vector 255.\n");
}

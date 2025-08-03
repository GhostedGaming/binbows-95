// ===============================
// PCI Configuration Access Module
// ===============================
// Provides low-level access to PCI configuration space
// via port-mapped I/O through CF8/CFC registers.

#include <stdint.h>
#include <io.h>
#include <serial.h>
#include <pci.h>

// -------------------------------
// Low-level PCI Config I/O
// -------------------------------

// Read a 32-bit value from PCI config space
uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) |
                                  (function << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

// Write a 32-bit value to PCI config space
void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) |
                                  (function << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

// Read a 16-bit value from PCI config space
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t data = pci_config_read_dword(bus, device, function, offset);
    uint8_t shift = (offset & 2) ? 16 : 0;
    return (uint16_t)((data >> shift) & 0xFFFF);
}

// Read an 8-bit value from PCI config space
uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t data = pci_config_read_dword(bus, device, function, offset);
    uint8_t shift = (offset & 3) * 8;
    return (data >> shift) & 0xFF;
}

// Write a 16-bit value to PCI config space
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t data = pci_config_read_dword(bus, device, function, offset);
    uint8_t shift = (offset & 2) * 8;
    data &= ~(0xFFFF << shift);
    data |= ((uint32_t)value << shift);
    pci_config_write_dword(bus, device, function, offset, data);
}

// Write an 8-bit value to PCI config space
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value) {
    uint32_t data = pci_config_read_dword(bus, device, function, offset);
    uint8_t shift = (offset & 3) * 8;
    data &= ~(0xFF << shift);
    data |= ((uint32_t)value << shift);
    pci_config_write_dword(bus, device, function, offset, data);
}

// -------------------------------
// PCI Device Identification
// -------------------------------

uint16_t get_vendor_id(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read_word(bus, device, function, 0x00);
}

uint16_t get_device_id(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read_word(bus, device, function, 0x02);
}

// -------------------------------
// PCI Enumeration Routines
// -------------------------------

// Check and print a specific function on a PCI device
void check_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor = get_vendor_id(bus, device, function);
    if (vendor == 0xFFFF) return; // Device doesn't exist

    uint16_t device_id = get_device_id(bus, device, function);
    serial_printf("Found PCI device: Bus %02x, Device %02x, Func %02x => Vendor: %04x, Device: %04x\n",
                  bus, device, function, vendor, device_id);

    uint8_t base_class = pci_config_read_byte(bus, device, function, 0x0B);
    uint8_t sub_class  = pci_config_read_byte(bus, device, function, 0x0A);

    // Recurse into secondary bus if device is PCI-to-PCI bridge
    if (base_class == 0x06 && sub_class == 0x04) {
        uint8_t secondary_bus = pci_config_read_byte(bus, device, function, 0x19);
        check_bus(secondary_bus);
    }
}

// Check all functions on a device (if multifunction)
void check_device(uint8_t bus, uint8_t device) {
    uint16_t vendor = get_vendor_id(bus, device, 0);
    if (vendor == 0xFFFF) return; // No device

    check_function(bus, device, 0);

    uint8_t header_type = pci_config_read_byte(bus, device, 0, 0x0E);
    if (header_type & 0x80) {
        for (uint8_t function = 1; function < 8; function++) {
            if (get_vendor_id(bus, device, function) != 0xFFFF) {
                check_function(bus, device, function);
            }
        }
    }
}

// Check all devices on a bus
void check_bus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; device++) {
        check_device(bus, device);
    }
}

// Entry point to scan all buses (start at bus 0)
void check_all_buses(void) {
    check_bus(0);
}
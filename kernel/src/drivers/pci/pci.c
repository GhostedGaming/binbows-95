#include <stdint.h>
#include <io.h>
#include <serial.h>
#include <pci.h>

#define MAX_PCI_DEVICES 256
pci_device pci_devices[MAX_PCI_DEVICES];
uint32_t pci_device_count = 0;

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) |
                                  (function << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) |
                                  (function << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t data = pci_config_read_dword(bus, device, function, offset);
    uint8_t shift = (offset & 2) ? 16 : 0;
    return (uint16_t)((data >> shift) & 0xFFFF);
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t data = pci_config_read_dword(bus, device, function, offset);
    uint8_t shift = (offset & 3) * 8;
    return (data >> shift) & 0xFF;
}

void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t data = pci_config_read_dword(bus, device, function, offset);
    uint8_t shift = (offset & 2) * 8;
    data &= ~(0xFFFF << shift);
    data |= ((uint32_t)value << shift);
    pci_config_write_dword(bus, device, function, offset, data);
}

void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value) {
    uint32_t data = pci_config_read_dword(bus, device, function, offset);
    uint8_t shift = (offset & 3) * 8;
    data &= ~(0xFF << shift);
    data |= ((uint32_t)value << shift);
    pci_config_write_dword(bus, device, function, offset, data);
}

uint16_t get_vendor_id(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read_word(bus, device, function, 0x00);
}

uint16_t get_device_id(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read_word(bus, device, function, 0x02);
}

uint32_t pci_read_bar(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_num) {
    return pci_config_read_dword(bus, device, function, 0x10 + (bar_num * 4));
}

void pci_write_bar(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_num, uint32_t value) {
    pci_config_write_dword(bus, device, function, 0x10 + (bar_num * 4), value);
}

uint32_t pci_get_bar_size(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_num) {
    uint32_t original = pci_read_bar(bus, device, function, bar_num);
    pci_write_bar(bus, device, function, bar_num, 0xFFFFFFFF);
    uint32_t size = pci_read_bar(bus, device, function, bar_num);
    pci_write_bar(bus, device, function, bar_num, original);
    
    if (original & 0x1) {
        size &= 0xFFFFFFFC;
    } else {
        size &= 0xFFFFFFF0;
    }
    
    return ~size + 1;
}

void pci_enable_bus_master(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t command = pci_config_read_word(bus, device, function, 0x04);
    command |= 0x04;
    pci_config_write_word(bus, device, function, 0x04, command);
}

void pci_disable_bus_master(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t command = pci_config_read_word(bus, device, function, 0x04);
    command &= ~0x04;
    pci_config_write_word(bus, device, function, 0x04, command);
}

void pci_enable_memory_space(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t command = pci_config_read_word(bus, device, function, 0x04);
    command |= 0x02;
    pci_config_write_word(bus, device, function, 0x04, command);
}

void pci_enable_io_space(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t command = pci_config_read_word(bus, device, function, 0x04);
    command |= 0x01;
    pci_config_write_word(bus, device, function, 0x04, command);
}

uint8_t pci_get_interrupt_line(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read_byte(bus, device, function, 0x3C);
}

uint8_t pci_get_interrupt_pin(uint8_t bus, uint8_t device, uint8_t function) {
    return pci_config_read_byte(bus, device, function, 0x3D);
}

void pci_add_device(uint8_t bus, uint8_t device, uint8_t function) {
    if (pci_device_count >= MAX_PCI_DEVICES) return;

    pci_device *dev = &pci_devices[pci_device_count];
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    dev->vendor_id = get_vendor_id(bus, device, function);
    dev->device_id = get_device_id(bus, device, function);
    dev->class_code = pci_config_read_byte(bus, device, function, 0x0B);
    dev->subclass = pci_config_read_byte(bus, device, function, 0x0A);
    dev->prog_if = pci_config_read_byte(bus, device, function, 0x09);
    dev->interrupt_line = pci_get_interrupt_line(bus, device, function);

    for (uint8_t i = 0; i < 6; i++) {
        dev->bar[i] = pci_read_bar(bus, device, function, i);
    }

    pci_device_count++;
}

pci_device* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

pci_device* pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

pci_device* pci_find_class_prog_if(uint8_t class_code, uint8_t subclass, uint8_t prog_if) {
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code && 
            pci_devices[i].subclass == subclass &&
            pci_devices[i].prog_if == prog_if) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

void check_bus(uint8_t bus);

void check_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor = get_vendor_id(bus, device, function);
    if (vendor == 0xFFFF) return;

    uint16_t device_id = get_device_id(bus, device, function);
    serial_printf("Found PCI device: Bus %02x, Device %02x, Func %02x => Vendor: %04x, Device: %04x\n",
                  bus, device, function, vendor, device_id);

    pci_add_device(bus, device, function);

    uint8_t base_class = pci_config_read_byte(bus, device, function, 0x0B);
    uint8_t sub_class  = pci_config_read_byte(bus, device, function, 0x0A);

    if (base_class == 0x06 && sub_class == 0x04) {
        uint8_t secondary_bus = pci_config_read_byte(bus, device, function, 0x19);
        check_bus(secondary_bus);
    }
}

void check_device(uint8_t bus, uint8_t device) {
    uint16_t vendor = get_vendor_id(bus, device, 0);
    if (vendor == 0xFFFF) return;

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

void check_bus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; device++) {
        check_device(bus, device);
    }
}

void check_all_buses(void) {
    uint8_t header_type = pci_config_read_byte(0, 0, 0, 0x0E);
    if ((header_type & 0x80) == 0) {
        check_bus(0);
    } else {
        for (uint8_t function = 0; function < 8; function++) {
            if (get_vendor_id(0, 0, function) != 0xFFFF) {
                check_bus(function);
            }
        }
    }
}

pci_device* pci_find_ahci_controller(void) {
    return pci_find_class_prog_if(0x01, 0x06, 0x01);
}

uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return pci_config_read_word(bus, slot, func, offset);
}

uint8_t pci_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return pci_config_read_byte(bus, slot, func, offset);
}

uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return pci_config_read_dword(bus, slot, func, offset);
}
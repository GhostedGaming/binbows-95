#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stddef.h>

// PCI I/O ports
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// PCI config space access macro
#define PCI_MAKE_ADDRESS(bus, slot, func, offset) \
    ((uint32_t)(0x80000000 | ((bus) << 16) | ((slot) << 11) | ((func) << 8) | ((offset) & 0xFC)))

// PCI layout constants
#define PCI_MAX_BUSES     256
#define PCI_MAX_DEVICES   32
#define PCI_MAX_FUNCTIONS 8

// PCI config space offsets
#define PCI_VENDOR_ID     0x00
#define PCI_DEVICE_ID     0x02
#define PCI_COMMAND       0x04
#define PCI_STATUS        0x06
#define PCI_REVISION_ID   0x08
#define PCI_PROG_IF       0x09
#define PCI_SUBCLASS      0x0A
#define PCI_CLASS_CODE    0x0B
#define PCI_HEADER_TYPE   0x0E
#define PCI_BAR0          0x10
#define PCI_CAP_PTR       0x34

// PCI header types
#define PCI_HEADER_TYPE_NORMAL 0x00
#define PCI_HEADER_TYPE_BRIDGE 0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02

// PCI class codes
#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_CLASS_NETWORK      0x02
#define PCI_CLASS_DISPLAY      0x03
#define PCI_CLASS_MULTIMEDIA   0x04
#define PCI_CLASS_MEMORY       0x05
#define PCI_CLASS_BRIDGE       0x06
#define PCI_CLASS_SERIAL_BUS   0x0C

// Structure to represent a PCI device
typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;
    uint8_t  header_type;
} pci_device_t;

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t get_vendor_id(uint8_t bus, uint8_t device, uint8_t function);
uint16_t get_device_id(uint8_t bus, uint8_t device, uint8_t function);
void check_function(uint8_t bus, uint8_t device, uint8_t function);
void check_device(uint8_t bus, uint8_t device);
void check_bus(uint8_t bus);
void check_all_buses(void);
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

#endif // PCI_H

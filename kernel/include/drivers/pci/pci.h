#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stddef.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_MAKE_ADDRESS(bus, slot, func, offset) \
    ((uint32_t)(0x80000000 | ((bus) << 16) | ((slot) << 11) | ((func) << 8) | ((offset) & 0xFC)))

#define PCI_MAX_BUSES     256
#define PCI_MAX_DEVICES   32
#define PCI_MAX_FUNCTIONS 8
#define MAX_PCI_DEVICES   256

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

#define PCI_HEADER_TYPE_NORMAL 0x00
#define PCI_HEADER_TYPE_BRIDGE 0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_CLASS_NETWORK      0x02
#define PCI_CLASS_DISPLAY      0x03
#define PCI_CLASS_MULTIMEDIA   0x04
#define PCI_CLASS_MEMORY       0x05
#define PCI_CLASS_BRIDGE       0x06
#define PCI_CLASS_SERIAL_BUS   0x0C

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint32_t bar[6];
    uint8_t interrupt_line;
} pci_device;

extern pci_device pci_devices[MAX_PCI_DEVICES];
extern uint32_t pci_device_count;

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
uint16_t pci_config_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_config_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_config_write_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);
uint16_t get_vendor_id(uint8_t bus, uint8_t device, uint8_t function);
uint16_t get_device_id(uint8_t bus, uint8_t device, uint8_t function);
uint32_t pci_read_bar(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_num);
void pci_write_bar(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_num, uint32_t value);
uint32_t pci_get_bar_size(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_num);
void pci_enable_bus_master(uint8_t bus, uint8_t device, uint8_t function);
void pci_disable_bus_master(uint8_t bus, uint8_t device, uint8_t function);
void pci_enable_memory_space(uint8_t bus, uint8_t device, uint8_t function);
void pci_enable_io_space(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_interrupt_line(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_interrupt_pin(uint8_t bus, uint8_t device, uint8_t function);
void pci_add_device(uint8_t bus, uint8_t device, uint8_t function);
pci_device* pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device* pci_find_class(uint8_t class_code, uint8_t subclass);
pci_device* pci_find_class_prog_if(uint8_t class_code, uint8_t subclass, uint8_t prog_if);
pci_device* pci_find_ahci_controller(void);
void check_function(uint8_t bus, uint8_t device, uint8_t function);
void check_device(uint8_t bus, uint8_t device);
void check_bus(uint8_t bus);
void check_all_buses(void);

#endif
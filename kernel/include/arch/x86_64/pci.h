#ifndef PCI_H
#define PCI_H

#include <stdint.h>

// Forward declarations for PCI config space functions
extern uint32_t read_config_u32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
extern void write_config_u32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

// Error codes matching the Rust PciError enum
typedef enum {
    PCI_SUCCESS = 0,
    PCI_DEVICE_NOT_FOUND,
    PCI_INVALID_OFFSET,
    PCI_IO_FAILURE
} pci_error_t;

/**
 * Information about a single PCI BAR (Base Address Register).
 * A BAR describes a region of memory or I/O space used by a PCI device.
 */
struct bar_info {
    /** The BAR index (0-5). Each PCI device can have up to 6 BARs. */
    uint8_t index;
    /** True if this BAR is for I/O space, false if for memory space. */
    bool is_io;
    /** True if this BAR is a 64-bit memory BAR (spans two BAR slots). */
    bool is_64bit;
    /** The base address of the region described by this BAR. */
    uint64_t address;
    /** The size (in bytes) of the region described by this BAR. */
    uint64_t size;
};

typedef struct bar_info bar_info_t;

#endif // PCI_H
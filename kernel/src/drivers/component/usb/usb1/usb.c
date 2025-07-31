#include <stdint.h>
#include <pci.h>
#include <io.h>
#include <serial.h>

void uhci_init() {
    serial_printf("Starting UHCI controller scan...\n");
    
    for (uint8_t bus = 0; bus < 4; ++bus) {
        serial_printf("Scanning bus %02x\n", bus);
        
        for (uint8_t device = 0; device < 32; ++device) {
            for (uint8_t function = 0; function < 8; ++function) {
                uint16_t vendor = pci_config_read_word(bus, device, function, 0x00);
                
                if (vendor == 0xFFFF) {
                    if (function == 0) {
                        serial_printf("  %02x:%02x.0 - No device\n", bus, device);
                    }
                    continue;
                }
                
                uint16_t device_id = pci_config_read_word(bus, device, function, 0x02);
                uint8_t class_code = pci_config_read_byte(bus, device, function, 0x0B);
                uint8_t subclass = pci_config_read_byte(bus, device, function, 0x0A);
                uint8_t prog_if = pci_config_read_byte(bus, device, function, 0x09);
                
                serial_printf("  %02x:%02x.%x - Vendor: 0x%04x, Device: 0x%04x, Class: 0x%02x, Subclass: 0x%02x, ProgIF: 0x%02x\n", 
                              bus, device, function, vendor, device_id, class_code, subclass, prog_if);
                
                if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x00) {
                    serial_printf("*** Found UHCI controller at %02x:%02x.%x ***\n", bus, device, function);
                    
                    serial_printf("Reading command register...\n");
                    uint16_t command = pci_config_read_word(bus, device, function, 0x04);
                    serial_printf("Current command register: 0x%04x\n", command);
                    
                    command |= 0x0005;  // Enable I/O space and bus mastering
                    serial_printf("Setting command register to: 0x%04x\n", command);
                    pci_config_write_word(bus, device, function, 0x04, command);
                    
                    serial_printf("Reading BAR4 (register 0x20)...\n");
                    uint32_t bar = pci_config_read_dword(bus, device, function, 0x20);
                    serial_printf("BAR4 value: 0x%08x\n", bar);
                    
                    if ((bar & 0x01) == 0) {
                        serial_printf("ERROR: BAR4 is not I/O space! (bit 0 = 0)\n");
                        serial_printf("Skipping this UHCI controller and continuing search...\n");
                        continue;
                    }
                    
                    uint16_t io_base = bar & 0xFFF0;
                    serial_printf("UHCI I/O Base: 0x%04x\n", io_base);
                    serial_printf("UHCI initialization complete!\n");
                    return;
                }
            }
        }
    }
    
    serial_printf("No usable UHCI controller found after scanning all buses!\n");
}
#include <stdint.h>
#include <pci.h>
#include <io.h>
#include <serial.h>

void uhci_init() {
    for (uint8_t bus = 0; bus < 256; ++bus) {
        for (uint8_t device = 0; device < 32; ++device) {
            for (uint8_t function = 0; function < 8; ++function) {
                uint16_t vendor = pci_config_read_word(bus, device, function, 0x00);
                if (vendor == 0xFFFF) continue;

                uint8_t class_code = pci_config_read_byte(bus, device, function, 0x0B);
                uint8_t subclass   = pci_config_read_byte(bus, device, function, 0x0A);
                uint8_t prog_if    = pci_config_read_byte(bus, device, function, 0x09);

                if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x00) {
                    serial_printf("Found UHCI controller at %02x:%02x.%x\n", bus, device, function);

                    uint16_t command = pci_config_read_word(bus, device, function, 0x04);
                    command |= 0x0005;
                    pci_config_write_word(bus, device, function, 0x04, command);

                    uint32_t bar = pci_config_read_dword(bus, device, function, 0x20);
                    if ((bar & 0x01) == 0) {
                        serial_printf("BAR4 is not I/O space!\n");
                        continue;
                    }
                    uint16_t io_base = bar & 0xFFF0;

                    serial_printf("UHCI I/O Base: 0x%04x\n", io_base);

                    return; 
                }
            }
        }
    }
}

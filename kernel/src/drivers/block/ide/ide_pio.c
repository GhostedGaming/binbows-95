#include <ide.h>
#include <stdint.h>
#include <io.h>

/* ============================================================================
 * LOW-LEVEL PORT I/O
 * ============================================================================ */

void ide_delay(uint8_t channel) {
    for (int i = 0; i < 4; i++) {
        ide_read(channel, ATA_REG_ALTSTATUS);
    }
}

uint8_t ide_read(uint8_t channel, uint8_t reg) {
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN | 0x02);
    
    uint16_t port = (reg < 0x08) ? channels[channel].base + reg
                                 : channels[channel].ctrl + (reg - 0x08);
    return inb(port);
}

void ide_write(uint8_t channel, uint8_t reg, uint8_t data) {
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN | 0x02);

    uint16_t port = (reg < 0x08) ? channels[channel].base + reg
                                 : channels[channel].ctrl + (reg - 0x08);
    outb(port, data);
}

void ide_read_buffer(uint8_t channel, uint8_t reg, void *buffer, uint32_t quads) {
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN | 0x02);

    uint16_t port = (reg < 0x08) ? channels[channel].base + reg
                                 : channels[channel].ctrl + (reg - 0x08);
    insw(port, buffer, quads);
}
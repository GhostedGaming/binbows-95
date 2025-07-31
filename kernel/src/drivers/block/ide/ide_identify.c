#include <ide.h>

/* ============================================================================
 * IDENTIFY DEVICE
 * ============================================================================ */

void ide_identify(uint8_t channel, uint8_t drive) {
    uint8_t dev = (drive & 1);
    uint16_t io = channels[channel].base;

    ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (dev << 4));
    timer_wait_ms(1);
    ide_write(channel, ATA_REG_SECCOUNT0, 0);
    ide_write(channel, ATA_REG_LBA0, 0);
    ide_write(channel, ATA_REG_LBA1, 0);
    ide_write(channel, ATA_REG_LBA2, 0);
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    timer_wait_ms(1);

    if (ide_read(channel, ATA_REG_STATUS) == 0) return;

    while (1) {
        uint8_t status = ide_read(channel, ATA_REG_STATUS);
        if ((status & ATA_SR_ERR)) return;
        if ((status & ATA_SR_DRQ)) break;
    }

    insw(io, ide_buf, 256);

    ide_devices[drive].Reserved     = 1;
    ide_devices[drive].Channel      = channel;
    ide_devices[drive].Drive        = dev;
    ide_devices[drive].Signature    = *((uint16_t *)(ide_buf + 0));
    ide_devices[drive].Capabilities = *((uint16_t *)(ide_buf + 98));
    ide_devices[drive].CommandSets  = *((uint32_t *)(ide_buf + 164));
    ide_devices[drive].Size         = *((uint32_t *)(ide_buf + 120));

    for (int k = 0; k < 40; k += 2) {
        ide_devices[drive].Model[k] = ide_buf[54 + k + 1];
        ide_devices[drive].Model[k + 1] = ide_buf[54 + k];
    }
    ide_devices[drive].Model[40] = '\0';
}
#include <ide.h>

/* ============================================================================
 * SECTOR READ
 * ============================================================================ */

int ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, void *buf) {
    if (drive > 3 || !ide_devices[drive].Reserved)
        return 1;

    uint8_t channel = ide_devices[drive].Channel;
    uint8_t slavebit = ide_devices[drive].Drive;
    uint16_t bus = channels[channel].base;
    uint8_t lba_mode = 0;
    uint8_t cmd;

    uint8_t lba_io[6];
    uint8_t head;

    uint16_t cyl, i;
    uint8_t err;

    if (lba >= 0x10000000) {
        lba_mode = 2;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = 0;
    } else {
        lba_mode = 1;
        lba_io[0] = (lba >> 0) & 0xFF;
        lba_io[1] = (lba >> 8) & 0xFF;
        lba_io[2] = (lba >> 16) & 0xFF;
        lba_io[3] = (lba >> 24) & 0x0F;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba >> 24) & 0x0F;
    }

    ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head);
    timer_wait_ms(1);

    ide_write(channel, ATA_REG_SECCOUNT0, numsects);
    ide_write(channel, ATA_REG_LBA0, lba_io[0]);
    ide_write(channel, ATA_REG_LBA1, lba_io[1]);
    ide_write(channel, ATA_REG_LBA2, lba_io[2]);
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    for (i = 0; i < numsects; i++) {
        if ((err = ide_polling(channel, 1))) return err;
        insw(bus, (uint16_t *)((uint8_t *)buf + i * 512), 256);
    }

    return 0;
}

/* ============================================================================
 * SECTOR WRITE
 * ============================================================================ */

int ide_write_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, const void *buf) {
    if (drive > 3 || !ide_devices[drive].Reserved)
        return 1;

    uint8_t channel = ide_devices[drive].Channel;
    uint8_t slavebit = ide_devices[drive].Drive;
    uint16_t bus = channels[channel].base;
    uint8_t lba_mode = 0;
    uint8_t cmd;

    uint8_t lba_io[6];
    uint8_t head;
    uint16_t i;
    uint8_t err;

    if (lba >= 0x10000000) {
        return 2;
    } else {
        lba_mode = 1;
        lba_io[0] = (lba >> 0) & 0xFF;
        lba_io[1] = (lba >> 8) & 0xFF;
        lba_io[2] = (lba >> 16) & 0xFF;
        lba_io[3] = (lba >> 24) & 0x0F;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba >> 24) & 0x0F;
    }

    ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head);
    timer_wait_ms(1);

    ide_write(channel, ATA_REG_SECCOUNT0, numsects);
    ide_write(channel, ATA_REG_LBA0, lba_io[0]);
    ide_write(channel, ATA_REG_LBA1, lba_io[1]);
    ide_write(channel, ATA_REG_LBA2, lba_io[2]);
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    for (i = 0; i < numsects; i++) {
        if ((err = ide_polling(channel, 0))) return err;
        outsw(bus, (const uint16_t *)((const uint8_t *)buf + i * 512), 256);
    }

    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    if ((err = ide_polling(channel, 0))) return err;

    return 0;
}

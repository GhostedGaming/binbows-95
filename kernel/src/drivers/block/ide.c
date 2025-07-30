#include <ide.h>
#include <io.h>
#include <serial.h>
#include <timer.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * GLOBALS
 * ============================================================================ */

struct ide_channel channels[2];
struct ide_device ide_devices[4];
uint8_t ide_buf[512];
static uint8_t ide_irq_invoked = 0;

/* ============================================================================
 * LOW-LEVEL PORT I/O
 * ============================================================================ */

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

/* ============================================================================
 * POLLING & WAITING
 * ============================================================================ */

void ide_wait_irq(uint8_t channel) {
    while (!ide_irq_invoked);
    ide_irq_invoked = 0;
}

uint8_t ide_polling(uint8_t channel, uint8_t advanced_check) {
    for (int i = 0; i < 4; i++)
        ide_read(channel, ATA_REG_ALTSTATUS);

    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY);

    if (advanced_check) {
        uint8_t state = ide_read(channel, ATA_REG_STATUS);

        if (state & ATA_SR_ERR) return 2;
        if (state & ATA_SR_DF)  return 1;
        if (!(state & ATA_SR_DRQ)) return 3;
    }

    return 0;
}

/* ============================================================================
 * IDENTIFY DEVICE
 * ============================================================================ */

static void ide_identify(uint8_t channel, uint8_t drive) {
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

/* ============================================================================
 * INITIALIZE
 * ============================================================================ */

void ide_initialize(void) {
    serial_printf("ide_initialize was called\n");
    channels[ATA_PRIMARY].base  = 0x1F0;
    channels[ATA_PRIMARY].ctrl  = 0x3F6;
    channels[ATA_PRIMARY].bmide = 0;
    channels[ATA_PRIMARY].nIEN  = 0;

    channels[ATA_SECONDARY].base  = 0x170;
    channels[ATA_SECONDARY].ctrl  = 0x376;
    channels[ATA_SECONDARY].bmide = 0;
    channels[ATA_SECONDARY].nIEN  = 0;

    ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

    for (int i = 0; i < 4; i++) {
        ide_devices[i].Reserved = 0;
        ide_identify(i / 2, i % 2);
        if (ide_devices[i].Reserved) {
            serial_printf("Found IDE drive %d: %s, Size: %u sectors\n",
                        i, ide_devices[i].Model, ide_devices[i].Size);
        }
    }
}

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
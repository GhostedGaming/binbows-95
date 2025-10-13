#include <ide.h>

/* ============================================================================
 * GLOBALS
 * ============================================================================ */

struct ide_channel channels[2];
struct ide_device ide_devices[4];
uint8_t ide_buf[512];

/* ============================================================================
 * INITIALIZE
 * ============================================================================ */

static int ide_device_exists(uint8_t channel, uint8_t drive) {
    if (channel > 1) return 0;
    if (drive > 1) return 0;
    ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (drive << 4));
    ide_delay(channel);
    
    uint8_t status = ide_read(channel, ATA_REG_STATUS);
    if (status == 0xFF || status == 0x00) {
        return 0;
    }
    
    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ide_delay(channel);
    
    status = ide_read(channel, ATA_REG_STATUS);
    if (status == 0x00) {
        return 0;
    }
    
    while (status & ATA_SR_BSY) {
        status = ide_read(channel, ATA_REG_STATUS);
    }
    
    if (status & ATA_SR_ERR) {
        return 0;
    }
    
    uint8_t cl = ide_read(channel, ATA_REG_LBA1);
    uint8_t ch = ide_read(channel, ATA_REG_LBA2);
    
    if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96)) {
        return 2;
    }
    
    if (cl == 0x00 && ch == 0x00) {
        return 1;
    }
    
    if (cl == 0x3C && ch == 0xC3) {
        return 3;
    }
    
    return 0;
}

void ide_initialize(void) {
    serial_printf("Initializing IDE driver\n");
    
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
        
        int channel = i / 2;
        int drive = i % 2;
        
        int device_type = ide_device_exists(channel, drive);
        
        if (device_type == 0) {
            serial_printf("No device at IDE %d:%d\n", channel, drive);
            continue;
        }
        
        if (device_type == 2) {
            serial_printf("ATAPI device at IDE %d:%d (skipping)\n", channel, drive);
            continue;
        }
        
        if (device_type == 3) {
            serial_printf("SATA device at IDE %d:%d (skipping)\n", channel, drive);
            continue;
        }
        
        ide_identify(channel, drive);
        
        if (ide_devices[i].Reserved) {
            serial_printf("Found IDE drive %d: %s, Size: %u sectors\n",
                        i, ide_devices[i].Model, ide_devices[i].Size);
        }
    }
}
/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */
uint64_t read_total_sectors(uint8_t drive_num) {
    if (drive_num >= 4) return 0;
    return ide_devices[drive_num].Size;
}
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
        ide_identify(i / 2, i % 2);
        if (ide_devices[i].Reserved) {
            serial_printf("Found IDE drive %d: %s, Size: %u sectors\n",
                        i, ide_devices[i].Model, ide_devices[i].Size);
        }
    }
}
#include <ide.h>

uint8_t ide_polling(uint8_t channel, uint8_t check) {
    for (int i = 0; i < 4; i++)
        ide_read(channel, ATA_REG_ALTSTATUS);

    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY);

    if (check) {
        uint8_t state = ide_read(channel, ATA_REG_STATUS);

        if (state & ATA_SR_ERR) return 2;
        if (state & ATA_SR_DF)  return 1;
        if (!(state & ATA_SR_DRQ)) return 3;
    }

    return 0;
}
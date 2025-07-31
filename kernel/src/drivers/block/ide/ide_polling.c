#include <ide.h>

static uint8_t ide_irq_invoked = 0;

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
#ifndef IDE_H
#define IDE_H

#include <io.h>
#include <serial.h>
#include <timer.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

// Channels
#define ATA_PRIMARY     0
#define ATA_SECONDARY   1

// Drives
#define ATA_MASTER      0
#define ATA_SLAVE       1

// Device Types
#define IDE_ATA         0
#define IDE_ATAPI       1

// Register Offsets
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT0   0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_COMMAND     0x07
#define ATA_REG_STATUS      0x07
#define ATA_REG_SECCOUNT1   0x08
#define ATA_REG_LBA3        0x09
#define ATA_REG_LBA4        0x0A
#define ATA_REG_LBA5        0x0B
#define ATA_REG_CONTROL     0x0C
#define ATA_REG_ALTSTATUS   0x0C
#define ATA_REG_DEVADDRESS  0x0D

// Status Flags
#define ATA_SR_BSY      0x80
#define ATA_SR_DRDY     0x40
#define ATA_SR_DF       0x20
#define ATA_SR_DSC      0x10
#define ATA_SR_DRQ      0x08
#define ATA_SR_CORR     0x04
#define ATA_SR_IDX      0x02
#define ATA_SR_ERR      0x01

// Error Flags
#define ATA_ER_BBK      0x80
#define ATA_ER_UNC      0x40
#define ATA_ER_MC       0x20
#define ATA_ER_IDNF     0x10
#define ATA_ER_MCR      0x08
#define ATA_ER_ABRT     0x04
#define ATA_ER_TK0NF    0x02
#define ATA_ER_AMNF     0x01

// Commands
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

struct ide_channel {
    uint16_t base;      // I/O Base
    uint16_t ctrl;      // Control Base
    uint16_t bmide;     // Bus Master IDE
    uint8_t  nIEN;      // No Interrupt
};

struct ide_device {
    uint8_t  Reserved;       // 0 (Empty) or 1 (This Drive really exists)
    uint8_t  Channel;        // 0 (Primary Channel) or 1 (Secondary Channel)
    uint8_t  Drive;          // 0 (Master Drive) or 1 (Slave Drive)
    uint16_t Type;           // 0: ATA, 1:ATAPI
    uint16_t Signature;      // Drive Signature
    uint16_t Capabilities;   // Features
    uint32_t CommandSets;    // Supported Command Sets
    uint32_t Size;           // Size in Sectors
    char     Model[41];      // Model in string
};

/* ============================================================================
 * GLOBALS
 * ============================================================================ */

extern struct ide_channel channels[2];
extern struct ide_device ide_devices[4];
extern uint8_t ide_buf[512];

/* ============================================================================
 * FUNCTIONS
 * ============================================================================ */

uint8_t ide_polling(uint8_t channel, uint8_t advanced_check);
uint8_t ide_read(uint8_t channel, uint8_t reg);
void ide_initialize(void);
void ide_identify(uint8_t channel, uint8_t drive);
void ide_wait_irq(uint8_t channel);
void ide_read_buffer(uint8_t channel, uint8_t reg, void *buffer, uint32_t quads);
void ide_write(uint8_t channel, uint8_t reg, uint8_t data);
int ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, void *buf);
int ide_write_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, const void *buf);
uint64_t read_total_sectors(uint8_t drive_num);

#endif // IDE_H
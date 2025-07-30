#ifndef IDE_H
#define IDE_H

/* ============================================================================
 * IDE/ATA Driver Header
 * * This header defines constants, structures, and function prototypes for
 * low-level IDE/ATA hardware interface operations.
 * ============================================================================ */

/* ============================================================================
 * ATA STATUS REGISTER FLAGS
 * ============================================================================ */
#define ATA_SR_BSY      0x80    // Busy
#define ATA_SR_DRDY     0x40    // Drive ready
#define ATA_SR_DF       0x20    // Drive write fault
#define ATA_SR_DSC      0x10    // Drive seek complete
#define ATA_SR_DRQ      0x08    // Data request ready
#define ATA_SR_CORR     0x04    // Corrected data
#define ATA_SR_IDX      0x02    // Index
#define ATA_SR_ERR      0x01    // Error

/* ============================================================================
 * ATA ERROR REGISTER FLAGS
 * ============================================================================ */
#define ATA_ER_BBK      0x80    // Bad block
#define ATA_ER_UNC      0x40    // Uncorrectable data
#define ATA_ER_MC       0x20    // Media changed
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // Media change request
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

/* ============================================================================
 * ATA COMMANDS
 * ============================================================================ */
// Read Commands
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_READ_DMA        0xC8
#define ATA_CMD_READ_DMA_EXT    0x25

// Write Commands
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_WRITE_DMA       0xCA
#define ATA_CMD_WRITE_DMA_EXT   0x35

// Cache Commands
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA

// Identification Commands
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1

// ATAPI Commands
#define ATA_CMD_PACKET          0xA0
#define ATAPI_CMD_READ          0xA8
#define ATAPI_CMD_EJECT         0x1B

/* ============================================================================
 * ATA REGISTER OFFSETS
 * ============================================================================ */
// Primary Registers (0x00-0x07)
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01    // Read-only
#define ATA_REG_FEATURES    0x01    // Write-only
#define ATA_REG_SECCOUNT0   0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_COMMAND     0x07    // Write-only
#define ATA_REG_STATUS      0x07    // Read-only

// Extended Registers (0x08-0x0B)
#define ATA_REG_SECCOUNT1   0x08
#define ATA_REG_LBA3        0x09
#define ATA_REG_LBA4        0x0A
#define ATA_REG_LBA5        0x0B

// Control Registers (0x0C-0x0D)
#define ATA_REG_CONTROL     0x0C    // Write-only
#define ATA_REG_ALTSTATUS   0x0C    // Read-only
#define ATA_REG_DEVADDRESS  0x0D

/* ============================================================================
 * IDENTIFY DATA OFFSETS
 * ============================================================================ */
#define ATA_IDENT_DEVICETYPE    0
#define ATA_IDENT_CYLINDERS     2
#define ATA_IDENT_HEADS         6
#define ATA_IDENT_SECTORS       12
#define ATA_IDENT_SERIAL        20
#define ATA_IDENT_MODEL         54
#define ATA_IDENT_CAPABILITIES  98
#define ATA_IDENT_FIELDVALID    106
#define ATA_IDENT_MAX_LBA       120
#define ATA_IDENT_COMMANDSETS   164
#define ATA_IDENT_MAX_LBA_EXT   200

/* ============================================================================
 * DEVICE TYPE AND CHANNEL DEFINITIONS
 * ============================================================================ */
// Device Types
#define IDE_ATA     0x00
#define IDE_ATAPI   0x01

// Drive Selection
#define ATA_MASTER  0x00
#define ATA_SLAVE   0x01

// Channel Selection
#define ATA_PRIMARY     0x00
#define ATA_SECONDARY   0x01

// Operation Direction
#define ATA_READ    0x00
#define ATA_WRITE   0x01

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * IDE Channel Register Configuration
 * Contains the I/O port addresses for each IDE channel
 */
struct IDEChannelRegisters {
    unsigned short base;    // I/O Base port
    unsigned short ctrl;    // Control Base port
    unsigned short bmide;   // Bus Master IDE port
    unsigned char nIEN;     // nIEN (No Interrupt) flag
};
// Add typedef for convenience
typedef struct IDEChannelRegisters ide_channel_t;


/**
 * IDE Device Information Structure
 * Stores identification and capability data for each IDE device
 */
struct ide_device {
    unsigned char Reserved;      // 0 (Empty) or 1 (Device exists)
    unsigned char Channel;       // 0 (Primary) or 1 (Secondary)
    unsigned char Drive;         // 0 (Master) or 1 (Slave)
    unsigned short Type;         // 0 (ATA) or 1 (ATAPI)
    unsigned short Signature;    // Drive Signature
    unsigned short Capabilities; // Feature flags
    unsigned int CommandSets;    // Supported command sets
    unsigned int Size;           // Size in sectors
    unsigned char Model[41];     // Model string (null-terminated)
};
// Add typedef for convenience
typedef struct ide_device ide_device_t;


/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */
extern ide_channel_t channels[2];               // Channel configurations
extern ide_device_t ide_devices[4];             // Device information array
extern unsigned char ide_buf[2048];             // Data transfer buffer
extern volatile unsigned char ide_irq_invoked;  // IRQ status flag
extern unsigned char atapi_packet[12];          // ATAPI command packet

/* ============================================================================
 * FUNCTION PROTOTYPES
 * ============================================================================ */

/**
 * Register Access Functions
 */
unsigned char ide_read(unsigned char channel, unsigned char reg);
void ide_write(unsigned char channel, unsigned char reg, unsigned char data);

/**
 * Bulk Data Transfer Functions
 */
void ide_read_buffer(unsigned char channel, unsigned char reg, 
                     unsigned int *buffer, unsigned int quads);

/**
 * Status and Error Handling Functions
 */
unsigned char ide_polling(unsigned char channel, unsigned int advanced_check);
unsigned char ide_print_error(unsigned int drive, unsigned char err);

/**
 * Device Management Functions (to be implemented)
 */
void ide_initialize(unsigned int BAR0, unsigned int BAR1, unsigned int BAR2, 
                    unsigned int BAR3, unsigned int BAR4);
unsigned char ide_ata_access(unsigned char direction, unsigned char drive, 
                             unsigned int lba, unsigned char numsects, 
                             unsigned short selector, unsigned int edi);
void ide_wait_irq(void);
void ide_irq(void);

#endif // IDE_H

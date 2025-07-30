#include <ide.h>
#include <io.h>
#include <serial.h>
#include <timer.h>
#include <stddef.h>

/* ============================================================================
 * CONSTANTS AND DEFINITIONS
 * ============================================================================ */

// Channel definitions
#define ATA_PRIMARY     0
#define ATA_SECONDARY   1

// Device type definitions
#define IDE_ATA         0
#define IDE_ATAPI       1

// ATAPI device signatures
#define ATAPI_SIG_LOW   0x14
#define ATAPI_SIG_HIGH  0xEB
#define ATAPI_SIG2_LOW  0x69
#define ATAPI_SIG2_HIGH 0x96

// Buffer size for identification data (128 words = 256 bytes)
#define IDE_IDENT_BUFFER_SIZE 128

/* ============================================================================
 * FUNCTION PROTOTYPES FOR INTERNAL USE
 * ============================================================================ */
void ide_write_buffer(unsigned char channel, unsigned char reg, 
                     unsigned int *buffer, unsigned int quads);

/* ============================================================================
 * REGISTER ACCESS FUNCTIONS
 * ============================================================================ */

/**
 * Read a single byte from an IDE register
 * * @param channel - IDE channel (0 or 1)
 * @param reg     - Register number to read from
 * @return        - Value read from the register
 * * Register mapping:
 * - 0x00-0x07: Primary registers (base + offset)
 * - 0x08-0x0B: Alternate status registers (base + reg - 0x06)
 * - 0x0C-0x0D: Control registers (ctrl + reg - 0x0A)
 * - 0x0E-0x15: Bus master registers (bmide + reg - 0x0E)
 */
unsigned char ide_read(unsigned char channel, unsigned char reg) {
    unsigned char result;
    
    // Validate channel parameter
    if (channel > 1) {
        return 0xFF; // Invalid channel
    }
    
    // Handle special control register access for alternate status registers
    if (reg >= 0x08 && reg <= 0x0B) {
        // Setting nIEN (No Interrupt Enable) bit to 0x80 disables interrupts for this channel.
        // This is kept as it actively prevents the device from asserting IRQs.
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    }
    
    // Determine port address based on register range
    if (reg < 0x08) {
        // Primary registers (0x00-0x07)
        result = inb(channels[channel].base + reg);
    } else if (reg < 0x0C) {
        // Alternate status registers (0x08-0x0B)
        result = inb(channels[channel].base + reg - 0x06);
    } else if (reg < 0x0E) {
        // Control registers (0x0C-0x0D)
        result = inb(channels[channel].ctrl + reg - 0x0A);
    } else if (reg < 0x16) {
        // Bus master registers (0x0E-0x15)
        result = inb(channels[channel].bmide + reg - 0x0E);
    } else {
        // Invalid register
        return 0xFF;
    }
    
    // Restore original control register state (nIEN)
    if (reg >= 0x08 && reg <= 0x0B) {
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    }
    
    return result;
}

/**
 * Write a single byte to an IDE register
 * * @param channel - IDE channel (0 or 1)
 * @param reg     - Register number to write to
 * @param data    - Data byte to write
 */
void ide_write(unsigned char channel, unsigned char reg, unsigned char data) {
    // Validate channel parameter
    if (channel > 1) {
        return; // Invalid channel
    }
    
    // Write to appropriate register bank
    if (reg < 0x08) {
        // Primary registers (0x00-0x07)
        outb(channels[channel].base + reg, data);
    } else if (reg < 0x0C) {
        // Alternate status registers (0x08-0x0B) - typically read-only
        // Consider if writing here is actually needed
        return; // or handle specially if write is required
    } else if (reg < 0x0E) {
        // Control registers (0x0C-0x0D)
        outb(channels[channel].ctrl + (reg - 0x0C), data);
    } else if (reg < 0x16) {
        // Bus master registers (0x0E-0x15)
        outb(channels[channel].bmide + (reg - 0x0E), data);
    }
    // Invalid register range - do nothing
}
/**
 * Write multiple 32-bit words to an IDE register using optimized assembly
 * * @param channel - IDE channel (0 or 1)
 * @param reg     - Register to write to
 * @param buffer  - Buffer containing data to write
 * @param quads   - Number of 32-bit words to write
 */
void ide_write_buffer(unsigned char channel, unsigned char reg, unsigned int *buffer,unsigned int quads) {
    // Validate parameters
    if (channel > 1 || buffer == NULL || quads == 0) {
        return;
    }
    
    // Set control register for alternate status access
    if (reg > 0x07 && reg < 0x0C) {
        // Setting nIEN (No Interrupt Enable) bit to 0x80 disables interrupts for this channel.
        // This is kept as it actively prevents the device from asserting IRQs.
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    }
    
    // Calculate the port address
    unsigned short port;
    if (reg < 0x08) {
        port = channels[channel].base + reg;
    } else if (reg < 0x0C) {
        port = channels[channel].base + reg - 0x06;
    } else if (reg < 0x0E) {
        port = channels[channel].ctrl + reg - 0x0A;
    } else if (reg < 0x16) {
        port = channels[channel].bmide + reg - 0x0E;
    } else {
        // Invalid register
        return;
    }
    
    // Use optimized assembly for bulk transfer (x86-64 compatible)
    __asm__ __volatile__ (
        "cld\n\t"                  // Clear direction flag (forward)
        "rep outsl\n\t"            // Repeat: output string long (32-bit)
        :                          // No output operands
        : "d" (port),              // Input: EDX = I/O port
          "S" (buffer),            // Input: RSI = source buffer
          "c" (quads)              // Input: ECX = repeat count
        : "memory"                 // Clobbered: memory
    );
    
    // Restore control register state
    if (reg > 0x07 && reg < 0x0C) {
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    }
}

/**
 * Read multiple 32-bit words from an IDE register using optimized assembly
 * * @param channel - IDE channel (0 or 1)
 * @param reg     - Register to read from
 * @param buffer  - Buffer to store the read data
 * @param quads   - Number of 32-bit words to read
 */
void ide_read_buffer(unsigned char channel, unsigned char reg, unsigned int *buffer,
                     unsigned int quads) {
    // Validate parameters
    if (channel > 1 || buffer == NULL || quads == 0) {
        return;
    }
    
    // Set control register for alternate status access
    if (reg > 0x07 && reg < 0x0C) {
        // Setting nIEN (No Interrupt Enable) bit to 0x80 disables interrupts for this channel.
        // This is kept as it actively prevents the device from asserting IRQs.
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    }
    
    // Calculate the port address
    unsigned short port;
    if (reg < 0x08) {
        port = channels[channel].base + reg;
    } else if (reg < 0x0C) {
        port = channels[channel].base + reg - 0x06;
    } else if (reg < 0x0E) {
        port = channels[channel].ctrl + reg - 0x0A;
    } else if (reg < 0x16) {
        port = channels[channel].bmide + reg - 0x0E;
    } else {
        // Invalid register
        return;
    }
    
    __asm__ __volatile__ (
        "cld\n\t"                  // Clear direction flag (forward)
        "rep insl\n\t"             // Repeat: input string long (32-bit)
        :                          // No output operands
        : "d" (port),              // Input: EDX = I/O port
          "D" (buffer),            // Input: RDI = destination buffer
          "c" (quads)              // Input: ECX = repeat count
        : "memory"                 // Clobbered: memory
    );
    
    // Restore control register state
    if (reg > 0x07 && reg < 0x0C) {
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    }
}

/* ============================================================================
 * STATUS POLLING AND ERROR CHECKING
 * ============================================================================ */

/**
 * Poll IDE device status and perform error checking
 * * @param channel        - IDE channel (0 or 1)
 * @param advanced_check - Enable additional error checking (non-zero)
 * @return               - Error code:
 * 0 = Success
 * 1 = Device fault
 * 2 = Error flag set
 * 3 = DRQ not set when expected
 * 4 = Invalid channel
 */
unsigned char ide_polling(unsigned char channel, unsigned int advanced_check) {
    // Validate channel parameter
    if (channel > 1) {
        return 4; // Invalid channel error
    }
    
    // Step 1: Delay 400 nanoseconds for BSY to be set
    // Reading alternate status register takes ~100ns, so loop 4 times
    for (int i = 0; i < 4; i++) {
        ide_read(channel, ATA_REG_ALTSTATUS);
    }

    // Step 2: Wait for BSY (Busy) flag to be cleared with timeout
    unsigned int timeout = 1000000; // Timeout counter
    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) {
        if (--timeout == 0) {
            return 5; // Timeout error
        }
        // Busy wait - device is processing command
    }

    // Step 3: Perform advanced error checking if requested
    if (advanced_check) {
        unsigned char state = ide_read(channel, ATA_REG_STATUS);

        // Check for error flag
        if (state & ATA_SR_ERR) {
            return 2; // Error occurred
        }

        // Check for device fault
        if (state & ATA_SR_DF) {
            return 1; // Device fault
        }

        // Check DRQ (Data Request) flag
        // After BSY=0, DF=0, ERR=0, DRQ should be set for data transfer
        if ((state & ATA_SR_DRQ) == 0) {
            return 3; // DRQ should be set but isn't
        }
    }

    return 0; // Success - no errors detected
}

/* ============================================================================
 * ERROR REPORTING FUNCTIONS
 * ============================================================================ */

/**
 * Print detailed error information for IDE operations
 * * @param drive - Drive number (0-3)
 * @param err   - Error code from previous operation
 * @return      - Processed error code
 */
unsigned char ide_print_error(unsigned int drive, unsigned char err) {
    if (err == 0) {
        return err; // No error to report
    }

    // Validate drive parameter
    if (drive > 3) {
        serial_printf("IDE: Invalid drive number\n");
        return err;
    }

    serial_printf("IDE:");
    
    if (err == 1) {
        serial_printf("- Device Fault\n     ");
        err = 19;
    } else if (err == 2) {
        // Read error register for detailed error information
        unsigned char st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);
        
        if (st & ATA_ER_AMNF) {
            serial_printf("- No Address Mark Found\n     ");
            err = 7;
        }
        if (st & ATA_ER_TK0NF) {
            serial_printf("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_ABRT) {
            serial_printf("- Command Aborted\n     ");
            err = 20;
        }
        if (st & ATA_ER_MCR) {
            serial_printf("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_IDNF) {
            serial_printf("- ID mark not Found\n     ");
            err = 21;
        }
        if (st & ATA_ER_MC) {
            serial_printf("- No Media or Media Error\n     ");
            err = 3;
        }
        if (st & ATA_ER_UNC) {
            serial_printf("- Uncorrectable Data Error\n     ");
            err = 22;
        }
        if (st & ATA_ER_BBK) {
            serial_printf("- Bad Sectors\n     ");
            err = 13;
        }
    } else if (err == 3) {
        serial_printf("- Reads Nothing\n     ");
        err = 23;
    } else if (err == 4) {
        serial_printf("- Write Protected\n     ");
        err = 8;
    }
    
    // Print device information
    serial_printf("- [%s %s] %s\n",
        (const char *[]){"Primary", "Secondary"}[ide_devices[drive].Channel],
        (const char *[]){"Master", "Slave"}[ide_devices[drive].Drive],
        ide_devices[drive].Model);

    return err;
}

/* ============================================================================
 * ATA ACCESS FUNCTIONS
 * ============================================================================ */

/**
 * Perform ATA read/write operations
 * * @param direction - 0 for read, 1 for write
 * @param drive     - Drive number (0-3)
 * @param lba       - Logical Block Address
 * @param numsects  - Number of sectors to transfer
 * @param selector  - Memory segment selector
 * @param edi       - Destination/source address
 * @return          - Error code (0 = success)
 */
unsigned char ide_ata_access(unsigned char direction, unsigned char drive, 
                             unsigned int lba, unsigned char numsects, 
                             unsigned short selector, unsigned int edi) {
    unsigned char lba_mode; // 0: CHS, 1: LBA28, 2: LBA48
    unsigned char dma = 0;  // 0: No DMA, 1: DMA
    unsigned char cmd;
    unsigned char lba_io[6];
    unsigned int channel = ide_devices[drive].Channel;
    unsigned int slavebit = ide_devices[drive].Drive;
    unsigned int bus = channels[channel].base;
    unsigned int words = 256; // Almost all ATA drives have sector size of 512 bytes
    unsigned short cyl, i;
    unsigned char head, sect, err;
    
    // Disable interrupts for the selected channel by setting nIEN bit to 0x02.
    // This actively prevents the device from asserting IRQs.
    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = 0x02);
    
    // Select drive mode
    if (lba >= 0x10000000) {
        // LBA48 mode
        lba_mode = 2;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB
        lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB
        head = 0; // Lower 4-bits of HDDEVSEL are not used here
    } else if (ide_devices[drive].Capabilities & 0x200) {
        // LBA28 mode
        lba_mode = 1;
        lba_io[0] = (lba & 0x00000FF) >> 0;
        lba_io[1] = (lba & 0x000FF00) >> 8;
        lba_io[2] = (lba & 0x0FF0000) >> 16;
        lba_io[3] = 0; // These registers are only used when LBA48
        lba_io[4] = 0; // These registers are only used when LBA48
        lba_io[5] = 0; // These registers are only used when LBA48
        head = (lba & 0xF000000) >> 24;
    } else {
        // CHS mode
        lba_mode = 0;
        sect = (lba % 63) + 1;
        cyl = (lba + 1 - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xFF;
        lba_io[2] = (cyl >> 8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba + 1 - sect) % (16 * 63) / (63); // Head number
    }
    
    // Wait for drive to be ready
    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY);
    
    // Select drive
    if (lba_mode == 0) {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head);
    } else {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head);
    }
    
    // Write parameters
    if (lba_mode == 2) {
        ide_write(channel, ATA_REG_SECCOUNT1, 0);
        ide_write(channel, ATA_REG_LBA3, lba_io[3]);
        ide_write(channel, ATA_REG_LBA4, lba_io[4]);
        ide_write(channel, ATA_REG_LBA5, lba_io[5]);
    }
    ide_write(channel, ATA_REG_SECCOUNT0, numsects);
    ide_write(channel, ATA_REG_LBA0, lba_io[0]);
    ide_write(channel, ATA_REG_LBA1, lba_io[1]);
    ide_write(channel, ATA_REG_LBA2, lba_io[2]);
    
    // Select command
    if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;   
    if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;   
    if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
    
    // Send command
    ide_write(channel, ATA_REG_COMMAND, cmd);
    
    if (dma) {
        // DMA transfer (not implemented in this example)
        return 4; // DMA not supported
    } else {
        // PIO transfer
        if (direction == 0) {
            // PIO Read
            for (i = 0; i < numsects; i++) {
                err = ide_polling(channel, 1);
                if (err) return err;
                __asm__ __volatile__ (
                    "cld\n\t"
                    "rep insw"
                    :
                    : "c"(words), "d"(bus), "D"((uint16_t *)edi)
                    : "memory"
                );
                edi += (words * 2);
            }
        } else {
            // PIO Write
            for (i = 0; i < numsects; i++) {
                err = ide_polling(channel, 0); // Polling without checking DRQ
                if (err) return err;
                __asm__ __volatile__ (
                    "cld\n\t"
                    "rep outsw"
                    :
                    : "c"(words), "d"(bus), "S"((uint16_t *)edi)
                    : "memory"
                );
                edi += (words * 2);
            }
            
            // Flush cache
            ide_write(channel, ATA_REG_COMMAND, (char[]){ATA_CMD_CACHE_FLUSH, ATA_CMD_CACHE_FLUSH,
                     ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
            ide_polling(channel, 0); // Polling
        }
    }
    
    return 0; // Success
}

/* ============================================================================
 * INITIALIZATION FUNCTIONS
 * ============================================================================ */

/**
 * Initialize IDE controller and detect connected devices
 * * @param BAR0 - Base Address Register 0 (Primary Command Block)
 * @param BAR1 - Base Address Register 1 (Primary Control Block)
 * @param BAR2 - Base Address Register 2 (Secondary Command Block)
 * @param BAR3 - Base Address Register 3 (Secondary Control Block)
 * @param BAR4 - Base Address Register 4 (Bus Master IDE)
 */
void ide_initialize(unsigned int BAR0, unsigned int BAR1, unsigned int BAR2, 
                   unsigned int BAR3, unsigned int BAR4) {
    int i, j, k, count = 0;

    // Step 1: Detect I/O Ports which interface IDE Controller
    channels[ATA_PRIMARY].base  = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
    channels[ATA_PRIMARY].ctrl  = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
    channels[ATA_SECONDARY].base = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
    channels[ATA_SECONDARY].ctrl = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
    channels[ATA_PRIMARY].bmide = (BAR4 & 0xFFFFFFFC) + 0; // Bus Master IDE
    channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8; // Bus Master IDE

    // Step 2: Disable IRQs for both primary and secondary channels.
    // Setting the nIEN (No Interrupt Enable) bit to 2 (0x02) prevents the device
    // from asserting interrupts. This effectively removes the need for IRQ handling.
    ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

    // Step 3: Detect ATA-ATAPI Devices
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            unsigned char err = 0, type = IDE_ATA, status;
            ide_devices[count].Reserved = 0; // Assuming that no drive here

            // (I) Select Drive
            ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive
            timer_wait_ms(1); // Wait 1ms for drive select to work

            // (II) Send ATA Identify Command
            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            timer_wait_ms(1); // Wait for command to be processed

            // (III) Polling
            if (ide_read(i, ATA_REG_STATUS) == 0) {
                continue; // If Status = 0, No Device
            }

            while (1) {
                status = ide_read(i, ATA_REG_STATUS);
                if ((status & ATA_SR_ERR)) {
                    err = 1;
                    break; // If Err, Device is not ATA
                }
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
                    break; // Everything is right
                }
            }

            // (IV) Probe for ATAPI Devices
            if (err != 0) {
                unsigned char cl = ide_read(i, ATA_REG_LBA1);
                unsigned char ch = ide_read(i, ATA_REG_LBA2);

                if (cl == ATAPI_SIG_LOW && ch == ATAPI_SIG_HIGH) {
                    type = IDE_ATAPI;
                } else if (cl == ATAPI_SIG2_LOW && ch == ATAPI_SIG2_HIGH) {
                    type = IDE_ATAPI;
                } else {
                    continue; // Unknown Type (may not be a device)
                }

                ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                timer_wait_ms(1);
            }

            // (V) Read Identification Space of the Device
            ide_read_buffer(i, ATA_REG_DATA, (unsigned int *)ide_buf, IDE_IDENT_BUFFER_SIZE);

            // (VI) Read Device Parameters
            ide_devices[count].Reserved = 1;
            ide_devices[count].Type = type;
            ide_devices[count].Channel = i;
            ide_devices[count].Drive = j;
            ide_devices[count].Signature = *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[count].Capabilities = *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[count].CommandSets = *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS));

            // (VII) Get Size
            if (ide_devices[count].CommandSets & (1 << 26)) {
                // Device uses 48-Bit Addressing
                ide_devices[count].Size = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
            } else {
                // Device uses CHS or 28-bit Addressing
                ide_devices[count].Size = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA));
            }

            // (VIII) String indicates model of device
            for (k = 0; k < 40; k += 2) {
                ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }
            ide_devices[count].Model[40] = 0; // Terminate String

            count++;
        }
    }

    // Step 4: Print Summary
    for (i = 0; i < 4; i++) {
        if (ide_devices[i].Reserved == 1) {
            serial_printf(" Found %s Drive %dGB - %s\n",
                (const char *[]){"ATA", "ATAPI"}[ide_devices[i].Type],
                ide_devices[i].Size / 1024 / 1024 / 2,
                ide_devices[i].Model);
        }
    }
}
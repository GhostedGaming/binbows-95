/*
 * Copyright (c) 2023, Gavin D'souza. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef SATA_H
#define SATA_H

#include <stdbool.h>
#include <stdint.h>

// Ref: https://www.intel.com/content/www/us/en/io/serial-ata/serial-ata-ahci-spec-rev1-3-1.html

// --- FIS (Frame Information Structure) Types ---
#define FIS_TYPE_REG_H2D 0x27    // Register FIS - Host to Device
#define FIS_TYPE_REG_D2H 0x34    // Register FIS - Device to Host
#define FIS_TYPE_DMA_ACT 0x39    // DMA Activate FIS - Device to Host
#define FIS_TYPE_DMA_SETUP 0x41  // DMA Setup FIS - Bi-directional
#define FIS_TYPE_DATA 0x46       // Data FIS - Bi-directional
#define FIS_TYPE_BIST 0x58       // BIST Activate FIS - Bi-directional
#define FIS_TYPE_PIO_SETUP 0x5F  // PIO Setup FIS - Device to Host
#define FIS_TYPE_DEV_BITS 0xA1   // Set Device Bits FIS - Host to Device

// --- ATA Commands ---
#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH_CACHE_EXT 0xEA

// --- Port Register Bits ---
#define HBA_PxCMD_ST 0x0001   // Start
#define HBA_PxCMD_FRE 0x0010  // FIS Receive Enable
#define HBA_PxCMD_FR 0x4000   // FIS Receive Running
#define HBA_PxCMD_CR 0x8000   // Command List Running

#define HBA_PxIS_TFES (1 << 30)  // Task File Error Status

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

// --- Register FIS - Host to Device ---

typedef struct {
    uint8_t fis_type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 3;
    uint8_t c : 1;
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;
    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;
    uint8_t rsv1[4];
} __attribute__((packed)) FIS_REG_H2D;

// --- HBA Port ---

typedef volatile struct {
    uint32_t clb;   // Command List Base Address
    uint32_t clbu;  // Command List Base Address Upper 32 bits
    uint32_t fb;    // FIS Base Address
    uint32_t fbu;   // FIS Base Address Upper 32 bits
    uint32_t is;    // Interrupt Status
    uint32_t ie;    // Interrupt Enable
    uint32_t cmd;   // Command and Status
    uint32_t rsv0;
    uint32_t tfd;   // Task File Data
    uint32_t sig;   // Signature
    uint32_t ssts;  // SATA Status
    uint32_t sctl;  // SATA Control
    uint32_t serr;  // SATA Error
    uint32_t sact;  // SATA Active
    uint32_t ci;    // Command Issue
    uint32_t sntf;  // SATA Notification
    uint32_t fbs;   // FIS-based Switching Control
    uint32_t rsv1[11];
    uint32_t vendor[4];
} HBA_PORT;

// --- HBA Memory ---

typedef volatile struct {
    uint32_t cap;      // Host Capabilities
    uint32_t ghc;      // Global Host Control
    uint32_t is;       // Interrupt Status
    uint32_t pi;       // Ports Implemented
    uint32_t vs;       // Version
    uint32_t ccc_ctl;  // Command Completion Coalescing Control
    uint32_t ccc_pts;  // Command Completion Coalescing Ports
    uint32_t em_loc;   // Enclosure Management Location
    uint32_t em_ctl;   // Enclosure Management Control
    uint32_t cap2;     // Host Capabilities Extended
    uint32_t bohc;     // BIOS/OS Handoff Control and Status
    uint8_t rsv[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
    HBA_PORT ports[32];
} HBA_MEM;

// --- Command Header ---

typedef struct {
    uint8_t cfl : 5;    // Command FIS Length
    uint8_t a : 1;      // ATAPI
    uint8_t w : 1;      // Write
    uint8_t p : 1;      // Prefetchable
    uint8_t r : 1;      // Reset
    uint8_t b : 1;      // BIST
    uint8_t c : 1;      // Clear Busy upon R_OK
    uint8_t rsv0 : 1;
    uint8_t pmp : 4;    // Port Multiplier Port
    uint16_t prdtl;     // Physical Region Descriptor Table Length
    volatile uint32_t prdbc;  // Physical Region Descriptor Byte Count
    uint32_t ctba;      // Command Table Base Address
    uint32_t ctbau;     // Command Table Base Address Upper 32 bits
    uint32_t rsv1[4];
} __attribute__((packed)) HBA_CMD_HEADER;

// --- Physical Region Descriptor Table Entry ---

typedef struct {
    uint32_t dba;   // Data Base Address
    uint32_t dbau;  // Data Base Address Upper 32 bits
    uint32_t rsv0;
    uint32_t dbc : 22;  // Data Byte Count
    uint32_t rsv1 : 9;
    uint32_t i : 1;     // Interrupt on Completion
} __attribute__((packed)) HBA_PRDT_ENTRY;

// --- Command Table ---

typedef struct {
    uint8_t cfis[64];         // Command FIS
    uint8_t acmd[16];         // ATAPI Command
    uint8_t rsv[48];
    HBA_PRDT_ENTRY prdt_entry[1];
} __attribute__((packed)) HBA_CMD_TBL;

// --- Function Prototypes ---

void ahci_init(void);
bool ahci_read(HBA_PORT *port, uint64_t start, uint32_t count, void *buf);
bool ahci_write(HBA_PORT *port, uint64_t start, uint32_t count, const void *buf);
bool ahci_identify(HBA_PORT *port, uint16_t *identify_data);

#endif  // SATA_H

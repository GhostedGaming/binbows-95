#include <sata.h>
#include <pci.h>
#include <serial.h>
#include <util.h>
#include <mem.h>

static HBA_MEM *s_abar = NULL;
HBA_MEM *abar = NULL;

static uint64_t get_phys_addr(void *virt) {
    if ((uint64_t)virt >= 0xffff800000000000) {
        return virt_to_phys(virt);
    } else {
        return kernel_virt_to_phys(virt);
    }
}

static void stop_cmd(HBA_PORT *port) {
    // Per AHCI spec 1.3.1 section 10.3.1, to stop a port:
    // 1. Clear PxCMD.ST to 0.
    port->cmd &= ~HBA_PxCMD_ST;

    // 2. Wait for PxCMD.CR to be 0.
    while (port->cmd & HBA_PxCMD_CR);

    // 3. Clear PxCMD.FRE to 0.
    port->cmd &= ~HBA_PxCMD_FRE;

    // 4. Wait for PxCMD.FR to be 0.
    while (port->cmd & HBA_PxCMD_FR);
}

static void start_cmd(HBA_PORT *port) {
    while (port->cmd & HBA_PxCMD_CR);

    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

static int find_free_cmd_slot(HBA_PORT *port) {
    uint32_t slots = port->sact | port->ci;
    int num_cmd_slots = ((s_abar->cap >> 8) & 0x1F) + 1;

    for (int i = 0; i < num_cmd_slots; i++) {
        if ((slots & 1) == 0) {
            return i;
        }
        slots >>= 1;
    }

    serial_printf("SATA: No free command slots on port.\n");
    return -1;
}

static void configure_port(HBA_PORT *port) {
    stop_cmd(port);

    // Allocate Command List
    void *cmd_list_base = kmalloc(sizeof(HBA_CMD_HEADER) * 32);
    memset(cmd_list_base, 0, sizeof(HBA_CMD_HEADER) * 32);
    uint64_t cmd_list_phys = get_phys_addr(cmd_list_base);
    port->clb = (uint32_t)cmd_list_phys;
    port->clbu = (uint32_t)(cmd_list_phys >> 32);

    // Allocate FIS
    void *fis_base = kmalloc(256);
    memset(fis_base, 0, 256);
    uint64_t fis_phys = get_phys_addr(fis_base);
    port->fb = (uint32_t)fis_phys;
    port->fbu = (uint32_t)(fis_phys >> 32);

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER *)cmd_list_base;
    for (int i = 0; i < 32; i++) {
        cmd_header[i].prdtl = 8;  // 8 PRDT entries

        void *cmd_tbl_base = kmalloc(sizeof(HBA_CMD_TBL) + (8 - 1) * sizeof(HBA_PRDT_ENTRY));
        memset(cmd_tbl_base, 0, sizeof(HBA_CMD_TBL) + (8 - 1) * sizeof(HBA_PRDT_ENTRY));
        uint64_t cmd_tbl_phys = get_phys_addr(cmd_tbl_base);
        cmd_header[i].ctba = (uint32_t)cmd_tbl_phys;
        cmd_header[i].ctbau = (uint32_t)(cmd_tbl_phys >> 32);
    }

    start_cmd(port);
}

void ahci_init(void) {
    pci_device *ahci_device = pci_find_class_prog_if(0x01, 0x06, 0x01);
    if (ahci_device == NULL) {
        serial_printf("SATA: No AHCI controller found.\n");
        return;
    }

    s_abar = (HBA_MEM *)(uintptr_t)pci_read_bar(ahci_device->bus, ahci_device->device, ahci_device->function, 5);
    abar = s_abar;
    serial_printf("SATA: AHCI controller found at 0x%p\n", s_abar);

    uint32_t ports_implemented = s_abar->pi;
    for (int i = 0; i < 32; i++) {
        if (ports_implemented & (1 << i)) {
            HBA_PORT *port = &s_abar->ports[i];

            uint32_t ssts = port->ssts;
            uint8_t det = ssts & 0x0F;
            uint8_t ipm = (ssts >> 8) & 0x0F;

            if (det != HBA_PORT_DET_PRESENT || ipm != HBA_PORT_IPM_ACTIVE) {
                continue;
            }

            serial_printf("SATA: Port %d detected.\n", i);
            configure_port(port);
        }
    }
}

bool ahci_read(HBA_PORT *port, uint64_t start, uint32_t count, void *buf) {
    size_t total_size = count * 512;
    void* bounce_buffer = kmalloc(total_size);
    if (!bounce_buffer) {
        serial_printf("SATA: ahci_read failed to allocate bounce buffer.\n");
        return false;
    }

    port->is = (uint32_t)-1;  // Clear interrupt status
    int slot = find_free_cmd_slot(port);
    if (slot == -1) {
        serial_printf("SATA: ahci_read failed to find free command slot.\n");
        kfree(bounce_buffer);
        return false;
    }

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER *)phys_to_virt(port->clb);
    cmd_header += slot;
    cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd_header->w = 0;  // Read
    cmd_header->prdtl = 1;

    HBA_CMD_TBL *cmd_tbl = (HBA_CMD_TBL *)phys_to_virt(cmd_header->ctba);
    memset(cmd_tbl->cfis, 0, sizeof(cmd_tbl->cfis));

    uint64_t phys_addr = get_phys_addr(bounce_buffer);
    cmd_tbl->prdt_entry[0].dba = (uint32_t)phys_addr;
    cmd_tbl->prdt_entry[0].dbau = (uint32_t)(phys_addr >> 32);
    ((uint32_t*)&cmd_tbl->prdt_entry[0])[3] = (total_size - 1) | (1u << 31);

    FIS_REG_H2D *cmd_fis = (FIS_REG_H2D *)(&cmd_tbl->cfis);
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->c = 1;
    cmd_fis->command = ATA_CMD_READ_DMA_EX;

    cmd_fis->lba0 = (uint8_t)start;
    cmd_fis->lba1 = (uint8_t)(start >> 8);
    cmd_fis->lba2 = (uint8_t)(start >> 16);
    cmd_fis->device = 1 << 6;  // LBA mode

    cmd_fis->lba3 = (uint8_t)(start >> 24);
    cmd_fis->lba4 = (uint8_t)(start >> 32);
    cmd_fis->lba5 = (uint8_t)(start >> 40);

    cmd_fis->countl = (uint8_t)count;
    cmd_fis->counth = (uint8_t)(count >> 8);

    while ((port->tfd & 0x88));

    port->ci = 1 << slot;

    while (1) {
        if ((port->ci & (1 << slot)) == 0) {
            break;
        }
        if (port->is & HBA_PxIS_TFES) {
            serial_printf("SATA: Read error on port. SERR=0x%x, TFD=0x%x\n", port->serr, port->tfd);
            kfree(bounce_buffer);
            return false;
        }
    }

    memcpy(buf, bounce_buffer, total_size);
    kfree(bounce_buffer);

    return true;
}

bool ahci_write(HBA_PORT *port, uint64_t start, uint32_t count, const void *buf) {
    size_t total_size = count * 512;
    void* bounce_buffer = kmalloc(total_size);
    if (!bounce_buffer) {
        serial_printf("SATA: ahci_write failed to allocate bounce buffer.\n");
        return false;
    }
    memcpy(bounce_buffer, buf, total_size);

    port->is = (uint32_t)-1;  // Clear interrupt status
    int slot = find_free_cmd_slot(port);
    if (slot == -1) {
        kfree(bounce_buffer);
        return false;
    }

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER *)phys_to_virt(port->clb);
    cmd_header += slot;
    cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd_header->w = 1;  // Write
    cmd_header->prdtl = 1;

    HBA_CMD_TBL *cmd_tbl = (HBA_CMD_TBL *)phys_to_virt(cmd_header->ctba);
    memset(cmd_tbl->cfis, 0, sizeof(cmd_tbl->cfis));

    uint64_t phys_addr = get_phys_addr(bounce_buffer);
    cmd_tbl->prdt_entry[0].dba = (uint32_t)phys_addr;
    cmd_tbl->prdt_entry[0].dbau = (uint32_t)(phys_addr >> 32);
    ((uint32_t*)&cmd_tbl->prdt_entry[0])[3] = (total_size - 1) | (1u << 31);

    FIS_REG_H2D *cmd_fis = (FIS_REG_H2D *)(&cmd_tbl->cfis);
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->c = 1;
    cmd_fis->command = ATA_CMD_WRITE_DMA_EX;

    cmd_fis->lba0 = (uint8_t)start;
    cmd_fis->lba1 = (uint8_t)(start >> 8);
    cmd_fis->lba2 = (uint8_t)(start >> 16);
    cmd_fis->device = 1 << 6;  // LBA mode

    cmd_fis->lba3 = (uint8_t)(start >> 24);
    cmd_fis->lba4 = (uint8_t)(start >> 32);
    cmd_fis->lba5 = (uint8_t)(start >> 40);

    cmd_fis->countl = (uint8_t)count;
    cmd_fis->counth = (uint8_t)(count >> 8);

    while ((port->tfd & 0x88));

    port->ci = 1 << slot;

    while (1) {
        if ((port->ci & (1 << slot)) == 0) {
            break;
        }
        if (port->is & HBA_PxIS_TFES) {
            serial_printf("SATA: Write error on port. SERR=0x%x, TFD=0x%x\n", port->serr, port->tfd);
            kfree(bounce_buffer);
            return false;
        }
    }

    cmd_header->prdtl = 0;
    cmd_header->w = 0;

    memset(cmd_fis, 0, sizeof(FIS_REG_H2D));
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->c = 1;
    cmd_fis->command = ATA_CMD_FLUSH_CACHE_EXT;

    while ((port->tfd & 0x88));

    port->ci = 1 << slot;
    while (1) {
        if ((port->ci & (1 << slot)) == 0) {
            break;
        }
        if (port->is & HBA_PxIS_TFES) {
            serial_printf("SATA: Flush cache error on port.\n");
            kfree(bounce_buffer);
            return false;
        }
    }

    kfree(bounce_buffer);
    return true;
}

bool ahci_identify(HBA_PORT *port, uint16_t *identify_data) {
    void* bounce_buffer = kmalloc(512);
    if (!bounce_buffer) {
        serial_printf("SATA: ahci_identify failed to allocate bounce buffer.\n");
        return false;
    }

    port->is = (uint32_t)-1;
    int slot = find_free_cmd_slot(port);
    if (slot == -1) {
        kfree(bounce_buffer);
        return false;
    }

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER *)phys_to_virt(port->clb);
    cmd_header += slot;
    cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd_header->w = 0; // Read
    cmd_header->prdtl = 1;

    HBA_CMD_TBL *cmd_tbl = (HBA_CMD_TBL *)phys_to_virt(cmd_header->ctba);
    memset(cmd_tbl->cfis, 0, sizeof(cmd_tbl->cfis));

    uint64_t phys_addr = get_phys_addr(bounce_buffer);
    cmd_tbl->prdt_entry[0].dba = (uint32_t)phys_addr;
    cmd_tbl->prdt_entry[0].dbau = (uint32_t)(phys_addr >> 32);
    ((uint32_t*)&cmd_tbl->prdt_entry[0])[3] = (512 - 1) | (1u << 31);

    FIS_REG_H2D *cmd_fis = (FIS_REG_H2D*)(&cmd_tbl->cfis);
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->c = 1;
    cmd_fis->command = ATA_CMD_IDENTIFY;
    cmd_fis->device = 0; // Master device

    while ((port->tfd & 0x88));

    port->ci = 1 << slot;

    while (1) {
        if ((port->ci & (1 << slot)) == 0) {
            break;
        }
        if (port->is & HBA_PxIS_TFES) {
            serial_printf("SATA: Identify error on port. SERR=0x%x, TFD=0x%x\n", port->serr, port->tfd);
            kfree(bounce_buffer);
            return false;
        }
    }

    memcpy(identify_data, bounce_buffer, 512);
    kfree(bounce_buffer);

    return true;
}
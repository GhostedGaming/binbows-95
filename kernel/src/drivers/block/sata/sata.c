#include <sata.h>
#include <io.h>
#include <shell.h>
#include <serial.h>
#include <mem.h>
#include <util.h>
#include <pci.h>
#include <stdbool.h>
#include <stdint.h>

HBA_MEM *abar = 0;

static inline void *align_pointer(void *ptr, size_t align) {
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t aligned = (p + (align - 1)) & ~(align - 1);
    return (void *)aligned;
}

static int check_type(HBA_PORT *port) {
    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;
    if (det != HBA_PORT_DET_PRESENT) return AHCI_DEV_NULL;
    if (ipm != HBA_PORT_IPM_ACTIVE) return AHCI_DEV_NULL;
    switch (port->sig) {
        case SATA_SIG_ATAPI: return AHCI_DEV_SATAPI;
        case SATA_SIG_SEMB: return AHCI_DEV_SEMB;
        case SATA_SIG_PM: return AHCI_DEV_PM;
        default: return AHCI_DEV_SATA;
    }
}

static int find_cmdslot(HBA_PORT *port) {
    uint32_t slots = (port->sact | port->ci);
    int cmdslots = ((abar->cap) >> 8) & 0x1F;
    for (int i = 0; i < cmdslots; i++) {
        if ((slots & 1) == 0) return i;
        slots >>= 1;
    }
    serial_printf("Cannot find free command list entry\n");
    return -1;
}

static void stop_cmd(HBA_PORT *port) {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    while (1) {
        if (port->cmd & HBA_PxCMD_FR) continue;
        if (port->cmd & HBA_PxCMD_CR) continue;
        break;
    }
}

static void start_cmd(HBA_PORT *port) {
    while (port->cmd & HBA_PxCMD_CR);
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

static void port_rebase(HBA_PORT *port, int portno) {
    (void)portno;
    stop_cmd(port);

    void *clb_raw = kmalloc(1024 + 1024);
    if (!clb_raw) return;
    void *clb_aligned = align_pointer(clb_raw, 1024);
    port->clb = (uint32_t)(uintptr_t)clb_aligned;
    port->clbu = 0;
    memset(clb_aligned, 0, 1024);

    void *fb_raw = kmalloc(256 + 256);
    if (!fb_raw) {
        kfree(clb_raw);
        return;
    }
    void *fb_aligned = align_pointer(fb_raw, 256);
    port->fb = (uint32_t)(uintptr_t)fb_aligned;
    port->fbu = 0;
    memset(fb_aligned, 0, 256);

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER *)(uintptr_t)port->clb;
    for (int i = 0; i < 32; i++) {
        cmdheader[i].prdtl = 8;
        void *ctba_raw = kmalloc(256 + 128);
        if (!ctba_raw) continue;
        void *ctba_aligned = align_pointer(ctba_raw, 128);
        cmdheader[i].ctba = (uint32_t)(uintptr_t)ctba_aligned;
        cmdheader[i].ctbau = 0;
        memset(ctba_aligned, 0, 256);
    }

    start_cmd(port);
}

static int ahci_identify(HBA_PORT *port, uint16_t *buf) {
    port->is = (uint32_t)-1;
    int slot = find_cmdslot(port);
    if (slot == -1) return 0;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER *)(uintptr_t)port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmdheader->w = 0;
    cmdheader->prdtl = 1;

    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *)(uintptr_t)cmdheader->ctba;
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    cmdtbl->prdt_entry[0].dba = (uint32_t)(uintptr_t)buf;
    cmdtbl->prdt_entry[0].dbc = 511;
    cmdtbl->prdt_entry[0].i = 1;

    FIS_REG_H2D *cmdfis = (FIS_REG_H2D *)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_IDENTIFY;
    cmdfis->device = 0;

    int spin = 0;
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) spin++;
    if (spin == 1000000) return 0;

    port->ci = 1 << slot;
    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) return 0;
    }
    if (port->is & HBA_PxIS_TFES) return 0;
    return 1;
}

static void probe_port(HBA_MEM *abar, int portno) {
    uint32_t pi = abar->pi;
    if (!(pi & (1 << portno))) return;

    int dt = check_type(&abar->ports[portno]);
    if (dt == AHCI_DEV_NULL) return;

    serial_printf("SATA port %d: ", portno);
    shell_printf("SATA port %d: ", portno);

    switch (dt) {
        case AHCI_DEV_SATA:
            serial_printf("SATA drive detected\n");
            shell_printf("SATA drive detected\n");
            break;
        case AHCI_DEV_SATAPI:
            serial_printf("SATAPI drive detected\n");
            shell_printf("SATAPI drive detected\n");
            return;
        case AHCI_DEV_SEMB:
            serial_printf("SEMB drive detected\n");
            shell_printf("SEMB drive detected\n");
            return;
        case AHCI_DEV_PM:
            serial_printf("PM drive detected\n");
            shell_printf("PM drive detected\n");
            return;
        default:
            return;
    }

    port_rebase(&abar->ports[portno], portno);

    uint16_t *identify_buf = (uint16_t *)kmalloc(512);
    if (!identify_buf) return;

    if (!ahci_identify(&abar->ports[portno], identify_buf)) {
        kfree(identify_buf);
        return;
    }

    char model[41];
    for (int i = 0; i < 20; i++) {
        model[i * 2] = identify_buf[27 + i] >> 8;
        model[i * 2 + 1] = identify_buf[27 + i] & 0xFF;
    }
    model[40] = '\0';
    for (int i = 39; i >= 0 && model[i] == ' '; i--) model[i] = '\0';

    serial_printf("  Model: %s\n", model);
    shell_printf("  Model: %s\n", model);

    uint64_t sectors = *(uint32_t *)&identify_buf[60];
    if (identify_buf[83] & (1 << 10)) sectors = *(uint64_t *)&identify_buf[100];
    uint64_t size_mb = (sectors * 512) / (1024 * 1024);

    serial_printf("  Size: %llu MB (%llu sectors)\n", size_mb, sectors);
    shell_printf("  Size: %llu MB (%llu sectors)\n", size_mb, sectors);

    kfree(identify_buf);
}

static int sata_scan_all_drives() {
    uint16_t vendor, device;
    int bus, slot, func;
    int found = 0;

    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            for (func = 0; func < 8; func++) {
                vendor = pci_read_word(bus, slot, func, 0);
                if (vendor == 0xFFFF) continue;

                uint8_t class_code = pci_read_byte(bus, slot, func, 0x0B);
                uint8_t subclass = pci_read_byte(bus, slot, func, 0x0A);
                uint8_t prog_if = pci_read_byte(bus, slot, func, 0x09);

                if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {
                    device = pci_read_word(bus, slot, func, 2);
                    serial_printf("Found AHCI controller: %04x:%04x at %02x:%02x.%x\n",
                        vendor, device, bus, slot, func);
                    shell_printf("Found AHCI controller: %04x:%04x at %02x:%02x.%x\n",
                        vendor, device, bus, slot, func);

                    uint32_t bar5 = pci_read_dword(bus, slot, func, 0x24);
                    abar = (HBA_MEM *)(uintptr_t)(bar5 & 0xFFFFFFF0);

                    serial_printf("AHCI Base Address: 0x%x\n", (uint32_t)(uintptr_t)abar);
                    shell_printf("AHCI Base Address: 0x%x\n", (uint32_t)(uintptr_t)abar);
                    serial_printf("AHCI Version: %x.%x\n", (abar->vs >> 16) & 0xFFFF, abar->vs & 0xFFFF);
                    shell_printf("AHCI Version: %x.%x\n", (abar->vs >> 16) & 0xFFFF, abar->vs & 0xFFFF);

                    found = 1;
                    goto end_search;
                }
            }
        }
    }

end_search:
    if (!found) {
        serial_printf("No AHCI controller found\n");
        shell_printf("No AHCI controller found\n");
        return -1;
    }

    serial_printf("\nScanning AHCI ports...\n");
    shell_printf("\nScanning AHCI ports...\n");
    for (int i = 0; i < 32; i++) probe_port(abar, i);
    return 0;
}

bool read_sectors(HBA_PORT *port, uint64_t startl, uint64_t starth, uint32_t count, void *buf) {
    port->is = (uint32_t)-1;
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER *)(uintptr_t)port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmdheader->w = 0;
    cmdheader->prdtl = (uint16_t)((count - 1) >> 4) + 1;

    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *)(uintptr_t)cmdheader->ctba;
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    int i;
    for (i = 0; i < cmdheader->prdtl - 1; i++) {
        cmdtbl->prdt_entry[i].dba = (uint32_t)(uintptr_t)buf;
        cmdtbl->prdt_entry[i].dbc = 8 * 1024 - 1;
        cmdtbl->prdt_entry[i].i = 1;
        buf = (uint8_t *)buf + 8 * 1024;
        count -= 16;
    }

    cmdtbl->prdt_entry[i].dba = (uint32_t)(uintptr_t)buf;
    cmdtbl->prdt_entry[i].dbc = ((count << 9) - 1);
    cmdtbl->prdt_entry[i].i = 1;

    FIS_REG_H2D *cmdfis = (FIS_REG_H2D *)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_READ_DMA_EX;
    cmdfis->lba0 = (uint8_t)(startl & 0xFF);
    cmdfis->lba1 = (uint8_t)((startl >> 8) & 0xFF);
    cmdfis->lba2 = (uint8_t)((startl >> 16) & 0xFF);
    cmdfis->device = 1 << 6;
    cmdfis->lba3 = (uint8_t)((startl >> 24) & 0xFF);
    cmdfis->lba4 = (uint8_t)(starth & 0xFF);
    cmdfis->lba5 = (uint8_t)((starth >> 8) & 0xFF);
    cmdfis->countl = (uint8_t)(count & 0xFF);
    cmdfis->counth = (uint8_t)((count >> 8) & 0xFF);
    cmdfis->control = 0;

    int spin = 0;
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) spin++;
    if (spin == 1000000) return false;
    port->ci = 1 << slot;
    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) return false;
    }
    if (port->is & HBA_PxIS_TFES) return false;
    return true;
}

bool write_sectors(HBA_PORT *port, uint64_t startl, uint64_t starth, uint32_t count, const void *buf) {
    port->is = (uint32_t)-1;
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER *)(uintptr_t)port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmdheader->w = 1;
    cmdheader->prdtl = (uint16_t)((count - 1) >> 4) + 1;

    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *)(uintptr_t)cmdheader->ctba;
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));

    int i;
    const uint8_t *data = (const uint8_t *)buf;
    for (i = 0; i < cmdheader->prdtl - 1; i++) {
        cmdtbl->prdt_entry[i].dba = (uint32_t)(uintptr_t)data;
        cmdtbl->prdt_entry[i].dbc = 8 * 1024 - 1;
        cmdtbl->prdt_entry[i].i = 1;
        data += 8 * 1024;
        count -= 16;
    }

    cmdtbl->prdt_entry[i].dba = (uint32_t)(uintptr_t)data;
    cmdtbl->prdt_entry[i].dbc = ((count << 9) - 1);
    cmdtbl->prdt_entry[i].i = 1;

    FIS_REG_H2D *cmdfis = (FIS_REG_H2D *)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_WRITE_DMA_EX;
    cmdfis->lba0 = (uint8_t)(startl & 0xFF);
    cmdfis->lba1 = (uint8_t)((startl >> 8) & 0xFF);
    cmdfis->lba2 = (uint8_t)((startl >> 16) & 0xFF);
    cmdfis->device = 1 << 6;
    cmdfis->lba3 = (uint8_t)((startl >> 24) & 0xFF);
    cmdfis->lba4 = (uint8_t)(starth & 0xFF);
    cmdfis->lba5 = (uint8_t)((starth >> 8) & 0xFF);
    cmdfis->countl = (uint8_t)(count & 0xFF);
    cmdfis->counth = (uint8_t)((count >> 8) & 0xFF);
    cmdfis->control = 0;

    int spin = 0;
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) spin++;
    if (spin == 1000000) return false;

    port->ci = 1 << slot;

    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) return false;
    }
    if (port->is & HBA_PxIS_TFES) return false;

    return true;
}

int sata_init() {
    if (sata_scan_all_drives() != 0) return -1;

    serial_printf("\nInitializing all SATA ports...\n");
    shell_printf("\nInitializing all SATA ports...\n");

    for (int i = 0; i < 32; i++) {
        uint32_t pi = abar->pi;
        if (!(pi & (1 << i))) continue;

        int dt = check_type(&abar->ports[i]);
        if (dt != AHCI_DEV_SATA) continue;

        serial_printf("Initializing port %d...\n", i);
        shell_printf("Initializing port %d...\n", i);

        port_rebase(&abar->ports[i], i);

        uint16_t *identify_buf = (uint16_t*)kmalloc(512);
        if (!identify_buf) continue;

        if (ahci_identify(&abar->ports[i], identify_buf)) {
            char model[41];
            for (int j = 0; j < 20; j++) {
                model[j * 2] = identify_buf[27 + j] >> 8;
                model[j * 2 + 1] = identify_buf[27 + j] & 0xFF;
            }
            model[40] = '\0';
            for (int j = 39; j >= 0 && model[j] == ' '; j--) model[j] = '\0';

            serial_printf("  Port %d Model: %s\n", i, model);
            shell_printf("  Port %d Model: %s\n", i, model);

            uint64_t sectors = *(uint32_t*)&identify_buf[60];
            if (identify_buf[83] & (1 << 10)) sectors = *(uint64_t*)&identify_buf[100];
            uint64_t size_mb = (sectors * 512) / (1024 * 1024);

            serial_printf("  Port %d Size: %llu MB (%llu sectors)\n", i, size_mb, sectors);
            shell_printf("  Port %d Size: %llu MB (%llu sectors)\n", i, size_mb, sectors);
        }

        kfree(identify_buf);
    }

    serial_printf("All SATA ports initialized.\n");
    shell_printf("All SATA ports initialized.\n");

    return 0;
}
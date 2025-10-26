#include <fat12.h>
#include <stdarg.h>

void fat_init(uint8_t drive, uint8_t reserved_sector_count, uint8_t num_fats, uint8_t fat_size_16) {
    uint8_t fat_sector[512];
    memset(fat_sector, 0, sizeof(fat_sector));

    fat_sector[0] = 0xF8;
    fat_sector[1] = 0xFF;
    fat_sector[2] = 0xFF;

    for (uint8_t fat_index = 0; fat_index < num_fats; fat_index++) {
        uint32_t fat_start = reserved_sector_count + fat_index * fat_size_16;
        
        ide_write_sectors(drive, 1, fat_start, fat_sector);
        
        memset(fat_sector, 0, sizeof(fat_sector));
        for (uint8_t sector = 1; sector < fat_size_16; sector++) {
            ide_write_sectors(drive, 1, fat_start + sector, fat_sector);
        }
    }

    serial_printf("FAT tables initialized\n");
}

void format_fat12(uint8_t drive, ...) {
    uint16_t bytes_per_sector = 512;
    uint8_t sectors_per_cluster = 1;
    uint16_t reserved_sector_count = 1;
    uint8_t num_fats = 2;
    uint16_t root_entry_count = 224;
    uint16_t fat_size_16 = 9;
    uint32_t total_sectors = ide_devices[drive].Size;

    va_list args;
    va_start(args, drive);
    char *oem_name = va_arg(args, char *);
    va_end(args);

    if (total_sectors == 0) {
        serial_printf("Error: Invalid drive size for drive %d\n", drive);
        return;
    }

    if (total_sectors > 4084) {
        sectors_per_cluster = 2;
    }
    if (total_sectors > 8168) {
        sectors_per_cluster = 4;
    }
    if (total_sectors > 16336) {
        sectors_per_cluster = 8;
    }

    uint32_t root_dir_sectors = ((root_entry_count * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t tmp1 = total_sectors - (reserved_sector_count + root_dir_sectors);
    uint32_t tmp2 = (256 * sectors_per_cluster) + num_fats;
    
    fat_size_16 = (uint16_t)((tmp1 + (tmp2 - 1)) / tmp2);
    
    if (fat_size_16 < 1) fat_size_16 = 1;
    
    uint32_t data_sectors = total_sectors - (reserved_sector_count + (num_fats * fat_size_16) + root_dir_sectors);
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    
    if (total_clusters > 4084) {
        serial_printf("Warning: Too many clusters (%u) for FAT12\n", total_clusters);
        while (total_clusters > 4084 && sectors_per_cluster < 128) {
            sectors_per_cluster *= 2;
            data_sectors = total_sectors - (reserved_sector_count + (num_fats * fat_size_16) + root_dir_sectors);
            total_clusters = data_sectors / sectors_per_cluster;
        }
    }

    serial_printf("Formatting FAT12:\n");
    serial_printf("  Total sectors: %u\n", total_sectors);
    serial_printf("  Sectors per cluster: %u\n", sectors_per_cluster);
    serial_printf("  Reserved sectors: %u\n", reserved_sector_count);
    serial_printf("  FAT size: %u sectors\n", fat_size_16);
    serial_printf("  Root entries: %u\n", root_entry_count);
    serial_printf("  Total clusters: %u\n", total_clusters);

    const uint8_t max_write = 100;
    uint8_t zero_buf[512 * max_write];
    memset(zero_buf, 0, sizeof(zero_buf));

    for (uint32_t lba = 0; lba < total_sectors;) {
        uint8_t sectors = (total_sectors - lba > max_write) ? max_write : (uint8_t)(total_sectors - lba);
        ide_write_sectors(drive, sectors, lba, zero_buf);
        lba += sectors;
    }

    uint8_t boot_sector[512];
    memset(boot_sector, 0, sizeof(boot_sector));
    
    bpb_t12* bpb = (bpb_t12*)boot_sector;
    
    bpb->jmp_boot[0] = 0xEB;
    bpb->jmp_boot[1] = 0x3C;
    bpb->jmp_boot[2] = 0x90;

    if (oem_name && strlen(oem_name) > 0) {
        memset(bpb->oem_name, ' ', 8);
        int len = strlen(oem_name);
        if (len > 8) len = 8;
        memcpy(bpb->oem_name, oem_name, len);
    } else {
        memcpy(bpb->oem_name, "mkfs.fat", 8);
    }

    bpb->bytes_per_sector = bytes_per_sector;
    bpb->sectors_per_cluster = sectors_per_cluster;
    bpb->reserved_sector_count = reserved_sector_count;
    bpb->num_fats = num_fats;
    bpb->root_entry_count = root_entry_count;
    
    if (total_sectors <= 0xFFFF) {
        bpb->total_sectors_16 = (uint16_t)total_sectors;
        bpb->total_sectors_32 = 0;
    } else {
        bpb->total_sectors_16 = 0;
        bpb->total_sectors_32 = total_sectors;
    }
    
    bpb->media = 0xF8;
    bpb->fat_size_16 = fat_size_16;
    
    bpb->sectors_per_track = 63;
    bpb->num_heads = 16;
    bpb->hidden_sectors = 0;
    
    bpb->drive_number = 0x80;
    bpb->reserved1 = 0;
    bpb->boot_signature = 0x29;

    bpb->volume_id = 0x12345678;
    
    memcpy(bpb->volume_label, "NO NAME    ", 11);
    
    memcpy(bpb->file_system_type, "FAT12   ", 8);

    boot_sector[510] = 0x55;
    boot_sector[511] = 0xAA;
    
    ide_write_sectors(drive, 1, 0, boot_sector);
    serial_printf("Boot sector written\n");

    uint8_t verify_sector[512];
    ide_read_sectors(drive, 1, 0, verify_sector);
    bpb_t12 *verify_bpb = (bpb_t12*)verify_sector;
    
    serial_printf("\nBPB Verification:\n");
    serial_printf("  Jump: %02X %02X %02X\n", 
        verify_bpb->jmp_boot[0], verify_bpb->jmp_boot[1], verify_bpb->jmp_boot[2]);
    serial_printf("  OEM: %.8s\n", verify_bpb->oem_name);
    serial_printf("  Bytes/Sector: %u\n", verify_bpb->bytes_per_sector);
    serial_printf("  Sectors/Cluster: %u\n", verify_bpb->sectors_per_cluster);
    serial_printf("  Reserved: %u\n", verify_bpb->reserved_sector_count);
    serial_printf("  FATs: %u\n", verify_bpb->num_fats);
    serial_printf("  Root Entries: %u\n", verify_bpb->root_entry_count);
    serial_printf("  Total Sectors (16): %u\n", verify_bpb->total_sectors_16);
    serial_printf("  Media: 0x%02X\n", verify_bpb->media);
    serial_printf("  FAT Size: %u\n", verify_bpb->fat_size_16);
    serial_printf("  Sectors/Track: %u\n", verify_bpb->sectors_per_track);
    serial_printf("  Heads: %u\n", verify_bpb->num_heads);
    serial_printf("  Boot Sig: 0x%02X\n", verify_bpb->boot_signature);
    serial_printf("  FS Type: %.8s\n", verify_bpb->file_system_type);

    fat_init(drive, reserved_sector_count, num_fats, fat_size_16);

    root_dir_sectors = ((root_entry_count * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t root_dir_lba = reserved_sector_count + (num_fats * fat_size_16);
    
    uint8_t zero_sector[512];
    memset(zero_sector, 0, sizeof(zero_sector));

    for (uint32_t i = 0; i < root_dir_sectors; i++) {
        ide_write_sectors(drive, 1, root_dir_lba + i, zero_sector);
    }

    serial_printf("Root directory initialized (%u sectors at LBA %u)\n", 
        root_dir_sectors, root_dir_lba);
    
    bpb_cached12 = false;
    
    serial_printf("\nFAT12 format complete! (mkfs.fat compatible)\n");
}
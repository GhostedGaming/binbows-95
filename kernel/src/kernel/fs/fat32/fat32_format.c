#include <fat32.h>

void fat32_init(uint8_t drive, uint32_t reserved_sector_count, uint8_t num_fats, uint32_t fat_size_32) {
    uint8_t fat_sector[512];
    memset(fat_sector, 0, sizeof(fat_sector));

    // Initialize first FAT32 sector with media descriptor and reserved clusters
    uint32_t* fat32 = (uint32_t*)fat_sector;
    fat32[0] = 0x0FFFFFF8;  // Media descriptor and reserved
    fat32[1] = 0xFFFFFFFF;  // Reserved
    fat32[2] = 0x0FFFFFFF;  // Root directory cluster (cluster 2) end-of-chain

    for (uint8_t fat_index = 0; fat_index < num_fats; fat_index++) {
        uint32_t fat_start = reserved_sector_count + fat_index * fat_size_32;

        // Write first sector with init
        ide_write_sectors(drive, 1, fat_start, fat_sector);

        // Clear remaining sectors
        memset(fat_sector, 0, sizeof(fat_sector));
        for (uint32_t sector = 1; sector < fat_size_32; sector++) {
            ide_write_sectors(drive, 1, fat_start + sector, fat_sector);
        }
    }

    serial_printf("FAT32 tables initialized\n");
}

void format_fat32(uint8_t drive) {
    const uint16_t bytes_per_sector = 512;
    uint8_t sectors_per_cluster = 8;
    const uint16_t reserved_sector_count = 32;
    const uint8_t num_fats = 2;
    const uint32_t root_cluster = 2;
    uint32_t fat_size_32 = 0;
    uint32_t total_sectors = ide_devices[drive].Size;

    if (total_sectors <= 0) {
        serial_printf("Error: Invalid drive size\n");
        return;
    }

    // Adjust sectors per cluster
    if (total_sectors > 32768 * 8) sectors_per_cluster = 64;
    else if (total_sectors > 16384 * 8) sectors_per_cluster = 32;
    else if (total_sectors > 8192 * 8) sectors_per_cluster = 16;
    else sectors_per_cluster = 8;

    // Calculate FAT size (approximation loop)
    uint32_t data_sectors = total_sectors - reserved_sector_count;
    uint32_t est_clusters = data_sectors / sectors_per_cluster;
    fat_size_32 = ((est_clusters + 2) * 4 + (bytes_per_sector - 1)) / bytes_per_sector;

    // Recompute with correct values
    data_sectors = total_sectors - (reserved_sector_count + num_fats * fat_size_32);
    est_clusters = data_sectors / sectors_per_cluster;
    fat_size_32 = ((est_clusters + 2) * 4 + (bytes_per_sector - 1)) / bytes_per_sector;

    if (est_clusters < 65525) {
        serial_printf("Too small for FAT32 (%u clusters), FAT16 may be better\n", est_clusters);
        return;
    }

    serial_printf("Formatting FAT32 drive: %u sectors, %u clusters, FAT size: %u sectors\n",
                  total_sectors, est_clusters, fat_size_32);

    // Clear the entire drive
    const uint8_t max_write = 100;
    uint8_t zero_buf[512 * max_write];
    memset(zero_buf, 0, sizeof(zero_buf));
    for (uint32_t lba = 0; lba < total_sectors;) {
        uint8_t sectors = (total_sectors - lba > max_write) ? max_write : (uint8_t)(total_sectors - lba);
        ide_write_sectors(drive, sectors, lba, zero_buf);
        lba += sectors;
    }

    // Build boot sector
    uint8_t boot_sector[512];
    memset(boot_sector, 0, sizeof(boot_sector));

    boot_sector[0] = 0xEB;
    boot_sector[1] = 0x58;
    boot_sector[2] = 0x90;
    memcpy(&boot_sector[3], "FAT32SYS", 8);

    uint8_t* bpb = &boot_sector[11];
    *(uint16_t*)(bpb + 0) = bytes_per_sector;
    *(uint8_t*)(bpb + 2) = sectors_per_cluster;
    *(uint16_t*)(bpb + 3) = reserved_sector_count;
    *(uint8_t*)(bpb + 5) = num_fats;
    *(uint16_t*)(bpb + 6) = 0; // root entries = 0 for FAT32
    *(uint16_t*)(bpb + 8) = 0; // total sectors 16
    *(uint8_t*)(bpb + 10) = 0xF8;
    *(uint16_t*)(bpb + 11) = 0; // fat_size_16 = 0
    *(uint16_t*)(bpb + 13) = 63;
    *(uint16_t*)(bpb + 15) = 255;
    *(uint32_t*)(bpb + 17) = 0; // hidden sectors
    *(uint32_t*)(bpb + 21) = total_sectors;

    *(uint32_t*)(bpb + 25) = fat_size_32;
    *(uint16_t*)(bpb + 29) = 0; // ext flags
    *(uint16_t*)(bpb + 31) = 0; // FS version
    *(uint32_t*)(bpb + 33) = root_cluster;
    *(uint16_t*)(bpb + 37) = 1; // FSInfo
    *(uint16_t*)(bpb + 39) = 6; // Backup boot sector
    *(uint8_t*)(bpb + 41) = 0x80;
    *(uint8_t*)(bpb + 42) = 0;
    *(uint8_t*)(bpb + 43) = 0x29;
    *(uint32_t*)(bpb + 44) = 0x87654321;
    memcpy(bpb + 48, "NO NAME    ", 11);
    memcpy(bpb + 59, "FAT32   ", 8);

    boot_sector[510] = 0x55;
    boot_sector[511] = 0xAA;

    ide_write_sectors(drive, 1, 0, boot_sector);
    serial_printf("FAT32 boot sector written\n");

    // FSInfo sector (sector 1)
    uint8_t fsinfo[512];
    memset(fsinfo, 0, sizeof(fsinfo));
    *(uint32_t*)(fsinfo + 0) = 0x41615252;
    *(uint32_t*)(fsinfo + 484) = 0x61417272;
    *(uint32_t*)(fsinfo + 488) = 0xFFFFFFFF;
    *(uint32_t*)(fsinfo + 492) = 0xFFFFFFFF;
    *(uint32_t*)(fsinfo + 508) = 0xAA550000 | 0x55;
    ide_write_sectors(drive, 1, 1, fsinfo);

    // Write backup boot sector (optional, at sector 6)
    ide_write_sectors(drive, 1, 6, boot_sector);
    ide_write_sectors(drive, 1, 7, fsinfo);

    // Initialize FATs
    fat32_init(drive, reserved_sector_count, num_fats, fat_size_32);

    // Clear root directory (start at cluster 2)
    uint32_t first_data_sector = reserved_sector_count + num_fats * fat_size_32;
    uint32_t root_dir_lba = first_data_sector;  // cluster 2 maps here
    memset(zero_buf, 0, sizeof(zero_buf));
    for (uint8_t i = 0; i < sectors_per_cluster; i++) {
        ide_write_sectors(drive, 1, root_dir_lba + i, zero_buf);
    }

    serial_printf("FAT32 root directory written\n");

    // Clear cache
    bpb_cached32 = false;

    serial_printf("FAT32 format complete\n");
}
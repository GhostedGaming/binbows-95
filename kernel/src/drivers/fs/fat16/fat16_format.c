#include <fat16.h>

void fat16_init(uint8_t drive, uint8_t reserved_sector_count, uint8_t num_fats, uint16_t fat_size_16) {
    uint8_t fat_sector[512];
    memset(fat_sector, 0, sizeof(fat_sector));

    // Initialize first FAT sector with media descriptor and reserved clusters
    uint16_t* fat16 = (uint16_t*)fat_sector;
    fat16[0] = 0xFFF8;  // Media descriptor (0xF8) + end-of-chain marker
    fat16[1] = 0xFFFF;  // Reserved cluster 1

    for (uint8_t fat_index = 0; fat_index < num_fats; fat_index++) {
        uint32_t fat_start = reserved_sector_count + fat_index * fat_size_16;
        
        // Write first sector with initialization
        ide_write_sectors(drive, 1, fat_start, fat_sector);
        
        // Clear remaining sectors
        memset(fat_sector, 0, sizeof(fat_sector));
        for (uint16_t sector = 1; sector < fat_size_16; sector++) {
            ide_write_sectors(drive, 1, fat_start + sector, fat_sector);
        }
    }

    serial_printf("FAT16 tables initialized\n");
}

void format_fat16(uint8_t drive) {
    uint16_t bytes_per_sector = 512;
    uint8_t sectors_per_cluster = 4; // good default for moderate size drives
    uint16_t reserved_sector_count = 1;
    uint8_t num_fats = 2;
    uint16_t root_entry_count = 512;
    uint16_t fat_size_16 = 0;
    int total_sectors = ide_devices[drive].Size;

    if (total_sectors <= 0) {
        serial_printf("Error: Invalid drive size\n");
        return;
    }

    // Adjust sectors per cluster based on drive size for FAT16
    if (total_sectors > 65536) {
        sectors_per_cluster = 8;
    } else if (total_sectors > 32768) {
        sectors_per_cluster = 4;
    } else if (total_sectors > 16384) {
        sectors_per_cluster = 2;
    } else {
        sectors_per_cluster = 1;
    }

    uint32_t root_dir_sectors = ((root_entry_count * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t data_sectors = total_sectors - (reserved_sector_count + root_dir_sectors);
    
    // Calculate FAT size for FAT16 (2 bytes per entry)
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    
    // Ensure we have at least 4085 clusters for FAT16 minimum
    if (total_clusters < 4085) {
        serial_printf("Drive too small for FAT16 (%u clusters), minimum 4085 required\n", total_clusters);
        return;
    }
    
    // Ensure we don't exceed FAT16 maximum of 65525 clusters
    if (total_clusters > 65525) {
        total_clusters = 65525;
    }
    
    uint32_t fat_bytes_needed = (total_clusters + 2) * 2;  // +2 for reserved clusters 0 and 1
    fat_size_16 = (fat_bytes_needed + bytes_per_sector - 1) / bytes_per_sector;

    if (fat_size_16 < 1) fat_size_16 = 1;

    serial_printf("Formatting FAT16 drive: %d sectors, %u clusters, FAT size: %d sectors\n", 
                  total_sectors, total_clusters, fat_size_16);

    // Clear entire drive
    const uint8_t max_write = 100;
    uint8_t zero_buf[512 * max_write];
    memset(zero_buf, 0, sizeof(zero_buf));
    for (uint32_t lba = 0; lba < (uint32_t)total_sectors;) {
        uint8_t sectors = (total_sectors - lba > max_write) ? max_write : (uint8_t)(total_sectors - lba);
        ide_write_sectors(drive, sectors, lba, zero_buf);
        lba += sectors;
    }

    uint8_t boot_sector[512];
    memset(boot_sector, 0, sizeof(boot_sector));

    // Jump instruction
    boot_sector[0] = 0xEB; 
    boot_sector[1] = 0x3C; 
    boot_sector[2] = 0x90;

    // OEM Name padded to 8 bytes
    memcpy(&boot_sector[3], "FAT16SYS", 8);

    // Build BPB
    uint8_t *bpb_ptr = &boot_sector[11];
    *(uint16_t*)(bpb_ptr + 0) = bytes_per_sector;        // bytes per sector
    *(uint8_t*)(bpb_ptr + 2) = sectors_per_cluster;      // sectors per cluster
    *(uint16_t*)(bpb_ptr + 3) = reserved_sector_count;   // reserved sectors
    *(uint8_t*)(bpb_ptr + 5) = num_fats;                 // number of FATs
    *(uint16_t*)(bpb_ptr + 6) = root_entry_count;        // root directory entries

    if (total_sectors > 0xFFFF) {
        *(uint16_t*)(bpb_ptr + 8) = 0;                   // total sectors 16 (0 if > 65535)
        *(uint32_t*)(bpb_ptr + 21) = total_sectors;      // total sectors 32
    } else {
        *(uint16_t*)(bpb_ptr + 8) = total_sectors;       // total sectors 16
        *(uint32_t*)(bpb_ptr + 21) = 0;                  // total sectors 32
    }

    *(uint8_t*)(bpb_ptr + 10) = 0xF8;                    // media descriptor for fixed disk
    *(uint16_t*)(bpb_ptr + 11) = fat_size_16;            // sectors per FAT
    *(uint16_t*)(bpb_ptr + 13) = 63;                     // sectors per track
    *(uint16_t*)(bpb_ptr + 15) = 16;                     // number of heads
    *(uint32_t*)(bpb_ptr + 17) = 0;                      // hidden sectors

    // Extended BPB for FAT16
    *(uint8_t*)(bpb_ptr + 25) = 0x80;                    // drive number
    *(uint8_t*)(bpb_ptr + 26) = 0;                       // reserved
    *(uint8_t*)(bpb_ptr + 27) = 0x29;                    // boot signature
    *(uint32_t*)(bpb_ptr + 28) = 0x12345678;             // volume ID

    memcpy(bpb_ptr + 32, "NO NAME    ", 11);            // volume label
    memcpy(bpb_ptr + 43, "FAT16   ", 8);                // file system type

    // Boot signature
    boot_sector[510] = 0x55;
    boot_sector[511] = 0xAA;

    ide_write_sectors(drive, 1, 0, boot_sector);
    serial_printf("FAT16 boot sector written\n");

    // Verify BPB
    uint8_t verify_sector[512];
    ide_read_sectors(drive, 1, 0, verify_sector);
    bpb_t16 *verify_bpb = (bpb_t16*)(verify_sector + 11);
    serial_printf("BPB verification after write:\n");
    serial_printf("  bytes_per_sector: %u\n", verify_bpb->bytes_per_sector);
    serial_printf("  sectors_per_cluster: %u\n", verify_bpb->sectors_per_cluster);
    serial_printf("  num_fats: %u\n", verify_bpb->num_fats);
    serial_printf("  fat_size_16: %u\n", verify_bpb->fat_size_16);
    serial_printf("  root_entry_count: %u\n", verify_bpb->root_entry_count);

    // Initialize FAT tables
    fat16_init(drive, reserved_sector_count, num_fats, fat_size_16);

    // Clear root directory
    uint32_t root_dir_lba = reserved_sector_count + (num_fats * fat_size_16);
    memset(zero_buf, 0, sizeof(zero_buf));
    for (uint32_t i = 0; i < root_dir_sectors; i++) {
        ide_write_sectors(drive, 1, root_dir_lba + i, zero_buf);
    }

    serial_printf("FAT16 root directory written\n");
    
    // Clear BPB cache to force re-read
    bpb_cached16 = false;
    
    serial_printf("FAT16 format complete\n");
}
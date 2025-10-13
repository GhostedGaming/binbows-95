#include <fat12.h>
#include <stdarg.h>

void fat_init(uint8_t drive, uint8_t reserved_sector_count, uint8_t num_fats, uint8_t fat_size_16) {
    uint8_t fat_sector[512];
    memset(fat_sector, 0, sizeof(fat_sector));

    fat_sector[0] = 0xF8;  // Media descriptor for hard disk
    fat_sector[1] = 0xFF;  // End of chain for cluster 0
    fat_sector[2] = 0xFF;  // End of chain for cluster 1

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
    uint8_t sectors_per_cluster = 4;      // Changed from 1 to 4 to match mkfs.fat
    uint8_t reserved_sector_count = 4;    // Changed from 1 to 4 to match mkfs.fat
    uint8_t num_fats = 2;
    uint16_t root_entry_count = 512;      // Changed from 224 to 512 for better compatibility
    uint16_t fat_size_16 = 12;            // Will be recalculated below
    uint32_t total_sectors = ide_devices[drive].Size;

    va_list args;
    va_start(args, drive);
    char *oem_name = va_arg(args, char *);
    va_end(args);

    if (total_sectors == 0) {
        serial_printf("Error: Invalid drive size for drive %d\n", drive);
        return;
    }

    // Calculate proper FAT size like mkfs.fat does
    uint32_t root_dir_sectors = ((root_entry_count * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t tmp1 = total_sectors - (reserved_sector_count + root_dir_sectors);
    uint32_t tmp2 = (256 * sectors_per_cluster) + num_fats;
    uint32_t tmp3 = (tmp1 + (tmp2 - 1)) / tmp2;
    
    // Calculate FAT size in sectors
    fat_size_16 = (tmp3 + 255) / 256;
    if (fat_size_16 < 1) fat_size_16 = 1;
    
    // Verify we're still in FAT12 range
    uint32_t data_sectors = total_sectors - (reserved_sector_count + (num_fats * fat_size_16) + root_dir_sectors);
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    
    if (total_clusters > 4085) {
        serial_printf("Warning: Too many clusters (%u) for FAT12, truncating\n", total_clusters);
        total_clusters = 4085;
    }

    serial_printf("Formatting drive: %d sectors, FAT size: %d sectors, clusters: %u\n", 
                  total_sectors, fat_size_16, total_clusters);

    // Clear the entire disk
    const uint8_t max_write = 100;
    uint8_t zero_buf[512 * max_write];
    memset(zero_buf, 0, sizeof(zero_buf));

    for (uint32_t lba = 0; lba < total_sectors;) {
        uint8_t sectors = (total_sectors - lba > max_write) ? max_write : (uint8_t)(total_sectors - lba);
        ide_write_sectors(drive, sectors, lba, zero_buf);
        lba += sectors;
    }

    // Create boot sector
    uint8_t boot_sector[512];
    memset(boot_sector, 0, sizeof(boot_sector));
    
    // Boot jump instruction (same as mkfs.fat)
    boot_sector[0] = 0xEB; 
    boot_sector[1] = 0x3C; 
    boot_sector[2] = 0x90;

    // OEM identifier - use "mkfs.fat" for compatibility or your custom name
    if (oem_name && strlen(oem_name) > 0) {
        memcpy(&boot_sector[3], oem_name, 8);
    } else {
        memcpy(&boot_sector[3], "mkfs.fat", 8);  // Match standard format
    }

    // BIOS Parameter Block
    bpb_t12* bpb = (bpb_t12*)(&boot_sector[11]);
    
    bpb->bytes_per_sector = bytes_per_sector;
    bpb->sectors_per_cluster = sectors_per_cluster;
    bpb->reserved_sector_count = reserved_sector_count;
    bpb->num_fats = num_fats;
    bpb->root_entry_count = root_entry_count;
    
    if (total_sectors > 0xFFFF) {
        bpb->total_sectors_16 = 0;
        bpb->total_sectors_32 = total_sectors;
    } else {
        bpb->total_sectors_16 = total_sectors;
        bpb->total_sectors_32 = 0;
    }
    
    bpb->media = 0xF8;                    // Fixed disk media descriptor
    bpb->fat_size_16 = fat_size_16;
    bpb->sectors_per_track = 32;          // Changed from 18 to 32 to match mkfs.fat
    bpb->num_heads = 2;
    bpb->hidden_sectors = 0;
    
    // Extended Boot Record
    bpb->drive_number = 0x80;             // Hard disk
    bpb->reserved1 = 0;
    bpb->boot_signature = 0x29;           // Extended boot signature
    bpb->volume_id = 0xCEFDE3DC;          // Random volume ID (you can generate this)
    memcpy(bpb->volume_label, "NO NAME    ", 11);
    memcpy(bpb->file_system_type, "FAT12   ", 8);

    // Boot signature
    boot_sector[510] = 0x55;
    boot_sector[511] = 0xAA;

    // Write boot sector
    ide_write_sectors(drive, 1, 0, boot_sector);
    serial_printf("Boot sector written\n");

    // Verify boot sector was written correctly
    uint8_t verify_sector[512];
    ide_read_sectors(drive, 1, 0, verify_sector);
    bpb_t12 *verify_bpb = (bpb_t12*)(verify_sector + 11);
    serial_printf("BPB verification after write:\n");
    serial_printf("  bytes_per_sector: %u\n", verify_bpb->bytes_per_sector);
    serial_printf("  sectors_per_cluster: %u\n", verify_bpb->sectors_per_cluster);
    serial_printf("  reserved_sector_count: %u\n", verify_bpb->reserved_sector_count);
    serial_printf("  num_fats: %u\n", verify_bpb->num_fats);
    serial_printf("  fat_size_16: %u\n", verify_bpb->fat_size_16);
    serial_printf("  sectors_per_track: %u\n", verify_bpb->sectors_per_track);

    // Initialize FAT tables
    fat_init(drive, reserved_sector_count, num_fats, fat_size_16);

    // Clear root directory
    root_dir_sectors = ((root_entry_count * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t root_dir_lba = reserved_sector_count + (num_fats * fat_size_16);
    uint8_t zero_sector[512];
    memset(zero_sector, 0, sizeof(zero_sector));

    for (uint32_t i = 0; i < root_dir_sectors; i++) {
        ide_write_sectors(drive, 1, root_dir_lba + i, zero_sector);
    }

    serial_printf("Root directory cleared (%u sectors)\n", root_dir_sectors);
    
    // Invalidate BPB cache
    bpb_cached12 = false;
    
    serial_printf("FAT12 format complete!\n");
}
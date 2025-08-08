#include <fat12.h>
#include <stdarg.h>

void fat_init(uint8_t drive, uint8_t reserved_sector_count, uint8_t num_fats, uint8_t fat_size_16) {
    uint8_t fat_sector[512];
    memset(fat_sector, 0, sizeof(fat_sector));

    // Initialize first FAT sector with media descriptor and end-of-chain markers
    fat_sector[0] = 0xF0;  // Media descriptor
    fat_sector[1] = 0xFF;  // End of chain for cluster 0
    fat_sector[2] = 0xFF;  // End of chain for cluster 1

    for (uint8_t fat_index = 0; fat_index < num_fats; fat_index++) {
        uint32_t fat_start = reserved_sector_count + fat_index * fat_size_16;
        
        // Write first sector with initialization
        ide_write_sectors(drive, 1, fat_start, fat_sector);
        
        // Clear remaining sectors
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
    uint8_t reserved_sector_count = 1;
    uint8_t num_fats = 2;
    uint16_t root_entry_count = 224;
    uint16_t fat_size_16 = 9;
    int total_sectors = ide_devices[drive].Size;

    va_list args;
    va_start(args, drive);

    char *oem_name = va_arg(args, char *);

    if (total_sectors <= 0) {
        serial_printf("%d\n", drive);
        serial_printf("Error: Invalid drive size\n");
        return;
    }

    // Calculate proper FAT size
    uint32_t root_dir_sectors = ((root_entry_count * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t data_sectors = total_sectors - (reserved_sector_count + root_dir_sectors);
    data_sectors -= (num_fats * fat_size_16);
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    
    if (total_clusters > 4085) {
        total_clusters = 4085;
    }
    
    uint32_t fat_bytes_needed = ((total_clusters + 2) * 3 + 1) / 2;
    fat_size_16 = (fat_bytes_needed + bytes_per_sector - 1) / bytes_per_sector;
    
    if (fat_size_16 < 1) fat_size_16 = 1;
    
    // Use standard FAT size for floppy-sized drives
    if (total_sectors <= 2880) {
        fat_size_16 = 9;
    }

    serial_printf("Formatting drive: %d sectors, FAT size: %d sectors\n", total_sectors, fat_size_16);

    // Clear the entire drive
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
    
    // Boot code jump
    boot_sector[0] = 0xEB; 
    boot_sector[1] = 0x3C; 
    boot_sector[2] = 0x90;

    // OEM name
    memcpy(&boot_sector[3], oem_name, 8);

    // BPB starts at offset 11
    uint8_t *bpb_ptr = &boot_sector[11];
    
    *(uint16_t*)(bpb_ptr + 0)  = bytes_per_sector;      // bytes_per_sector
    *(uint8_t*)(bpb_ptr + 2)   = sectors_per_cluster;   // sectors_per_cluster
    *(uint16_t*)(bpb_ptr + 3)  = reserved_sector_count; // reserved_sector_count
    *(uint8_t*)(bpb_ptr + 5)   = num_fats;              // num_fats
    *(uint16_t*)(bpb_ptr + 6)  = root_entry_count;      // root_entry_count
    
    // Total sectors (16-bit or 32-bit)
    if (total_sectors > 0xFFFF) {
        *(uint16_t*)(bpb_ptr + 8)  = 0;                 // total_sectors_16
        *(uint32_t*)(bpb_ptr + 21) = total_sectors;     // total_sectors_32
    } else {
        *(uint16_t*)(bpb_ptr + 8)  = total_sectors;     // total_sectors_16
        *(uint32_t*)(bpb_ptr + 21) = 0;                 // total_sectors_32
    }
    
    *(uint8_t*)(bpb_ptr + 10)  = 0xF0;                  // media
    *(uint16_t*)(bpb_ptr + 11) = fat_size_16;           // fat_size_16
    *(uint16_t*)(bpb_ptr + 13) = 18;                    // sectors_per_track
    *(uint16_t*)(bpb_ptr + 15) = 2;                     // num_heads
    *(uint32_t*)(bpb_ptr + 17) = 0;                     // hidden_sectors
    
    // Extended BPB
    *(uint8_t*)(bpb_ptr + 25)  = 0x80;                  // drive_number
    *(uint8_t*)(bpb_ptr + 26)  = 0;                     // reserved1
    *(uint8_t*)(bpb_ptr + 27)  = 0x29;                  // boot_signature
    *(uint32_t*)(bpb_ptr + 28) = 0x12345678;            // volume_id
    memcpy(bpb_ptr + 32, "NO NAME    ", 11);           // volume_label
    memcpy(bpb_ptr + 43, "FAT12   ", 8);               // file_system_type

    // Boot signature
    boot_sector[510] = 0x55;
    boot_sector[511] = 0xAA;

    // Write boot sector
    ide_write_sectors(drive, 1, 0, boot_sector);
    serial_printf("BPB sector written\n");

    // Verify boot sector
    uint8_t verify_sector[512];
    ide_read_sectors(drive, 1, 0, verify_sector);
    bpb_t12 *verify_bpb = (bpb_t12*)(verify_sector + 11);
    serial_printf("BPB verification after write:\n");
    serial_printf("  bytes_per_sector: %u\n", verify_bpb->bytes_per_sector);
    serial_printf("  sectors_per_cluster: %u\n", verify_bpb->sectors_per_cluster);
    serial_printf("  num_fats: %u\n", verify_bpb->num_fats);
    serial_printf("  fat_size_16: %u\n", verify_bpb->fat_size_16);

    // Initialize FAT tables
    fat_init(drive, reserved_sector_count, num_fats, fat_size_16);

    // Initialize root directory
    root_dir_sectors = ((root_entry_count * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t root_dir_lba = reserved_sector_count + (num_fats * fat_size_16);
    uint8_t zero_sector[512];
    memset(zero_sector, 0, sizeof(zero_sector));

    for (uint32_t i = 0; i < root_dir_sectors; i++) {
        ide_write_sectors(drive, 1, root_dir_lba + i, zero_sector);
    }

    serial_printf("Root_dir written\n");
    
    // Clear BPB cache to force re-read
    bpb_cached12 = false;
}
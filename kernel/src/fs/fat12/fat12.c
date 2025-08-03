#include <fat12.h>
#include <ide.h>
#include <mem.h>
#include <util.h>

// Global BPB cache to avoid repeated reads
static bpb_t cached_bpb;
static bool bpb_cached = false;
static uint8_t cached_drive = 0xFF;

static int read_bpb(uint8_t drive, bpb_t *bpb) {
    // Use cached BPB if available for the same drive
    if (bpb_cached && cached_drive == drive) {
        memcpy(bpb, &cached_bpb, sizeof(bpb_t));
        return 0;
    }
    
    uint8_t boot_sector[512];
    if (ide_read_sectors(drive, 1, 0, boot_sector) != 0) {
        return -1;
    }
    
    // Verify boot signature
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        serial_printf("Invalid boot signature: 0x%02X%02X\n", boot_sector[511], boot_sector[510]);
        return -1;
    }
    
    memcpy(bpb, boot_sector + 11, sizeof(bpb_t));
    
    // Cache the BPB
    memcpy(&cached_bpb, bpb, sizeof(bpb_t));
    bpb_cached = true;
    cached_drive = drive;
    
    return 0;
}

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

void format_fat12(uint8_t drive) {
    uint16_t bytes_per_sector = 512;
    uint8_t sectors_per_cluster = 1;
    uint8_t reserved_sector_count = 1;
    uint8_t num_fats = 2;
    uint16_t root_entry_count = 224;
    uint16_t fat_size_16 = 9;
    int total_sectors = ide_devices[drive].Size;

    if (total_sectors <= 0) {
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
    memcpy(&boot_sector[3], "MSDOS5.0", 8);

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
    bpb_t *verify_bpb = (bpb_t*)(verify_sector + 11);
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
    bpb_cached = false;
}

uint16_t fat12_alloc_clusters(uint8_t drive, uint16_t count) {
    bpb_t bpb;
    uint8_t fat[512 * 12];  // Support up to 12 sectors of FAT
    
    if (read_bpb(drive, &bpb) != 0) {
        serial_printf("Failed to read BPB in fat12_alloc_clusters\n");
        return 0;
    }
    
    if (!validate_bpb(&bpb)) {
        serial_printf("Invalid BPB in fat12_alloc_clusters\n");
        return 0;
    }
    
    // Read all FAT sectors
    if (ide_read_sectors(drive, bpb.fat_size_16, bpb.reserved_sector_count, fat) != 0) {
        serial_printf("Failed to read FAT in fat12_alloc_clusters\n");
        return 0;
    }
    
    uint32_t fat_bytes = bpb.fat_size_16 * bpb.bytes_per_sector;
    uint16_t max_clusters = (fat_bytes * 2) / 3;
    
    if (max_clusters > 4085) max_clusters = 4085;
    
    if (max_clusters < 3) {
        serial_printf("Error: Too few clusters available\n");
        return 0;
    }
    
    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;
    uint16_t allocated = 0;
    
    // Find and allocate free clusters
    for (uint16_t cluster = 2; cluster < max_clusters && allocated < count; cluster++) {
        if (fat12_read_entry(fat, cluster) == 0x000) {
            if (first_cluster == 0) {
                first_cluster = cluster;
            } else {
                fat12_write_entry(fat, prev_cluster, cluster);
            }
            prev_cluster = cluster;
            allocated++;
        }
    }
    
    if (allocated == count && prev_cluster != 0) {
        // Mark last cluster as end-of-chain
        fat12_write_entry(fat, prev_cluster, 0xFFF);
        
        // Write FAT back to disk
        fat12_write_fat(drive, bpb.num_fats, bpb.fat_size_16, fat, bpb.reserved_sector_count);
        return first_cluster;
    }
    
    serial_printf("Failed to allocate %u clusters (only allocated %u)\n", count, allocated);
    return 0;
}

uint16_t fat12_read_entry(const uint8_t* fat, uint16_t cluster) {
    if (cluster < 2) return 0xFFF;  // Invalid cluster
    
    uint32_t offset = cluster + (cluster / 2);
    uint16_t value = *(uint16_t*)(fat + offset);

    return (cluster & 1) ? (value >> 4) : (value & 0x0FFF);
}

void fat12_write_entry(uint8_t* fat, uint16_t cluster, uint16_t value) {
    if (cluster < 2) return;  // Invalid cluster
    
    uint32_t offset = cluster + (cluster / 2);
    uint16_t* entry = (uint16_t*)(fat + offset);
    uint16_t current = *entry;

    if (cluster & 1) {
        *entry = (value << 4) | (current & 0x000F);
    } else {
        *entry = (current & 0xF000) | (value & 0x0FFF);
    }
}

int fat12_find_free_cluster(uint8_t* fat, uint16_t max_clusters) {
    for (uint16_t cluster = 2; cluster < max_clusters; cluster++) {
        if (fat12_read_entry(fat, cluster) == 0x000) {
            return cluster;
        }
    }
    return -1;
}

void fat12_write_fat(uint8_t drive, uint8_t num_fats, uint16_t fat_size_16, const uint8_t* fat_data, uint8_t reserved_sector_count) {
    // Write all FAT copies
    for (uint8_t i = 0; i < num_fats; i++) {
        uint32_t fat_start = reserved_sector_count + i * fat_size_16;
        for (uint16_t s = 0; s < fat_size_16; s++) {
            ide_write_sectors(drive, 1, fat_start + s, fat_data + s * 512);
        }
    }
}

void fat12_format_filename(const char* filename, char* fat_name) {
    memset(fat_name, ' ', 11);
    fat_name[11] = '\0';  // Null terminate for debugging
    
    int name_len = 0;
    int ext_len = 0;
    const char* dot = strchr(filename, '.');
    
    if (dot) {
        name_len = dot - filename;
        if (name_len > 8) name_len = 8;
        ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        
        memcpy(fat_name, filename, name_len);
        memcpy(fat_name + 8, dot + 1, ext_len);
    } else {
        name_len = strlen(filename);
        if (name_len > 8) name_len = 8;
        memcpy(fat_name, filename, name_len);
    }
    
    // Convert to uppercase
    for (int i = 0; i < 11; i++) {
        if (fat_name[i] >= 'a' && fat_name[i] <= 'z') {
            fat_name[i] = fat_name[i] - 'a' + 'A';
        }
    }
}

int fat12_find_free_root_dir_entry(uint8_t drive, bpb_t* bpb) {
    uint32_t root_dir_sectors = ((bpb->root_entry_count * 32) + (bpb->bytes_per_sector - 1)) / bpb->bytes_per_sector;
    uint32_t root_dir_lba = bpb->reserved_sector_count + bpb->num_fats * bpb->fat_size_16;

    uint8_t sector[512];
    for (uint32_t i = 0; i < root_dir_sectors; i++) {
        if (ide_read_sectors(drive, 1, root_dir_lba + i, sector) != 0) {
            continue;
        }
        
        for (uint16_t offset = 0; offset < 512; offset += 32) {
            if (sector[offset] == 0x00 || sector[offset] == 0xE5) {
                return i * (512 / 32) + (offset / 32);
            }
        }
    }
    return -1;
}

void fat12_write_dir_entry(uint8_t drive, bpb_t* bpb, int index, const char* filename, uint16_t first_cluster, uint32_t size) {
    uint32_t lba = bpb->reserved_sector_count + bpb->num_fats * bpb->fat_size_16;
    uint32_t entries_per_sector = bpb->bytes_per_sector / 32;
    uint32_t sector_index = index / entries_per_sector;
    uint32_t entry_offset = (index % entries_per_sector) * 32;

    uint8_t sector[512];
    if (ide_read_sectors(drive, 1, lba + sector_index, sector) != 0) {
        serial_printf("Failed to read directory sector\n");
        return;
    }
    
    fat12_dir_entry_t* entry = (fat12_dir_entry_t*)(sector + entry_offset);
    memset(entry, 0, 32);

    fat12_format_filename(filename, entry->name);
    
    entry->attr = ATTR_ARCHIVE;
    entry->first_cluster_low = first_cluster;
    entry->first_cluster_high = 0;
    entry->file_size = size;

    if (ide_write_sectors(drive, 1, lba + sector_index, sector) != 0) {
        serial_printf("Failed to write directory entry\n");
    }
}

void fat12_write_clusters(uint8_t drive, uint16_t first_cluster, const uint8_t *data, uint32_t size) {
    bpb_t bpb;
    uint8_t fat[512 * 12];
    
    if (read_bpb(drive, &bpb) != 0) {
        serial_printf("Failed to read BPB in fat12_write_clusters\n");
        return;
    }
    
    if (!validate_bpb(&bpb)) {
        serial_printf("Invalid BPB in fat12_write_clusters\n");
        return;
    }
    
    if (ide_read_sectors(drive, bpb.fat_size_16, bpb.reserved_sector_count, fat) != 0) {
        serial_printf("Failed to read FAT in fat12_write_clusters\n");
        return;
    }
    
    uint32_t root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
    uint32_t data_start_lba = bpb.reserved_sector_count + (bpb.num_fats * bpb.fat_size_16) + root_dir_sectors;
    uint32_t cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;
    
    uint16_t current_cluster = first_cluster;
    uint32_t remaining = size;
    const uint8_t *data_ptr = data;
    
    while (remaining > 0 && current_cluster >= 2 && current_cluster < 0xFF8) {
        uint32_t lba = data_start_lba + (current_cluster - 2) * bpb.sectors_per_cluster;
        uint32_t to_write = (remaining > cluster_size) ? cluster_size : remaining;
        
        uint8_t cluster_data[cluster_size];
        memset(cluster_data, 0, cluster_size);
        memcpy(cluster_data, data_ptr, to_write);
        
        if (ide_write_sectors(drive, bpb.sectors_per_cluster, lba, cluster_data) != 0) {
            serial_printf("Failed to write cluster %u\n", current_cluster);
            break;
        }
        
        data_ptr += to_write;
        remaining -= to_write;
        
        if (remaining > 0) {
            current_cluster = fat12_read_entry(fat, current_cluster);
        } else {
            break;
        }
    }
}

int fat12_write_file(uint8_t drive, const char* filename, const uint8_t* data, uint32_t size) {
    bpb_t bpb;
    
    if (read_bpb(drive, &bpb) != 0) {
        serial_printf("Failed to read BPB in fat12_write_file\n");
        return -1;
    }
    
    if (!validate_bpb(&bpb)) {
        serial_printf("Invalid BPB in fat12_write_file\n");
        return -1;
    }
    
    uint32_t cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;
    uint16_t clusters_needed = (size + cluster_size - 1) / cluster_size;
    
    if (clusters_needed == 0) clusters_needed = 1;
    
    uint16_t first_cluster = fat12_alloc_clusters(drive, clusters_needed);
    if (first_cluster == 0) {
        serial_printf("No free clusters available\n");
        return -1;
    }
    
    int dir_entry = fat12_find_free_root_dir_entry(drive, &bpb);
    if (dir_entry == -1) {
        serial_printf("No free directory entry\n");
        return -1;
    }
    
    fat12_write_dir_entry(drive, &bpb, dir_entry, filename, first_cluster, size);
    fat12_write_clusters(drive, first_cluster, data, size);
    
    serial_printf("File written: %s size: %u clusters: %u\n", filename, size, clusters_needed);
    return 0;
}

void format_filename_for_compare(const char* filename, char* fat_name) {
    fat12_format_filename(filename, fat_name);
}

bool validate_bpb(bpb_t *bpb) {
    if (bpb->bytes_per_sector == 0 || bpb->bytes_per_sector != 512) {
        serial_printf("Invalid bytes_per_sector: %u\n", bpb->bytes_per_sector);
        return false;
    }
    if (bpb->sectors_per_cluster == 0) {
        serial_printf("Invalid sectors_per_cluster: %u\n", bpb->sectors_per_cluster);
        return false;
    }
    if (bpb->num_fats == 0) {
        serial_printf("Invalid num_fats: %u\n", bpb->num_fats);
        return false;
    }
    if (bpb->fat_size_16 == 0) {
        serial_printf("Invalid fat_size_16: %u\n", bpb->fat_size_16);
        return false;
    }
    if (bpb->root_entry_count == 0) {
        serial_printf("Invalid root_entry_count: %u\n", bpb->root_entry_count);
        return false;
    }
    return true;
}

bool fat12_read_file(uint8_t drive, const char *filename, uint8_t *buffer, uint32_t *size_out) {
    bpb_t bpb;
    
    if (read_bpb(drive, &bpb) != 0) {
        serial_printf("Failed to read BPB\n");
        return false;
    }
    
    if (!validate_bpb(&bpb)) {
        serial_printf("Invalid BPB - filesystem may not be formatted properly\n");
        return false;
    }
    
    uint32_t root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
    uint32_t fat_size = bpb.fat_size_16;
    uint32_t root_dir_lba = bpb.reserved_sector_count + (bpb.num_fats * fat_size);
    uint32_t data_start_lba = root_dir_lba + root_dir_sectors;
    
    char fat_filename[12];  // 11 + null terminator
    format_filename_for_compare(filename, fat_filename);
    
    serial_printf("Looking for file: ");
    for (int i = 0; i < 11; i++) {
        write_serial_char(fat_filename[i]);
    }
    write_serial_char('\n');

    uint8_t sector[512];
    for (uint32_t i = 0; i < root_dir_sectors; i++) {
        if (ide_read_sectors(drive, 1, root_dir_lba + i, sector) != 0) {
            serial_printf("Failed to read root directory sector %u\n", i);
            continue;
        }

        for (uint32_t j = 0; j < 512; j += 32) {
            fat12_dir_entry_t *entry = (fat12_dir_entry_t *)&sector[j];

            if (entry->name[0] == 0x00) {
                serial_printf("End of directory reached\n");
                return false;
            }
            
            if (entry->name[0] == 0xE5) {
                continue;
            }

            if ((entry->attr & ATTR_DIRECTORY) || (entry->attr & ATTR_VOLUME_ID)) {
                continue;
            }

            if (memcmp(entry->name, fat_filename, 11) == 0) {
                serial_printf("Found file! Size: %u bytes, First cluster: %u\n", 
                             entry->file_size, entry->first_cluster_low);
                
                uint16_t cluster = entry->first_cluster_low;
                uint32_t file_size = entry->file_size;
                uint32_t bytes_read = 0;
                uint32_t cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;

                // Read FAT table
                uint8_t fat_table[512 * 12];
                if (ide_read_sectors(drive, fat_size, bpb.reserved_sector_count, fat_table) != 0) {
                    serial_printf("Failed to read FAT table\n");
                    return false;
                }

                while (cluster >= 2 && cluster < 0xFF8 && bytes_read < file_size) {
                    uint32_t lba = data_start_lba + (cluster - 2) * bpb.sectors_per_cluster;
                    uint32_t bytes_to_read = (file_size - bytes_read > cluster_size) ? 
                                           cluster_size : (file_size - bytes_read);
                    
                    uint8_t cluster_data[cluster_size];
                    if (ide_read_sectors(drive, bpb.sectors_per_cluster, lba, cluster_data) != 0) {
                        serial_printf("Failed to read cluster %u\n", cluster);
                        break;
                    }
                    
                    memcpy(buffer + bytes_read, cluster_data, bytes_to_read);
                    bytes_read += bytes_to_read;

                    // Get next cluster from FAT
                    cluster = fat12_read_entry(fat_table, cluster);
                    serial_printf("Next cluster: %u\n", cluster);
                }

                *size_out = bytes_read;
                serial_printf("Successfully read %u bytes\n", bytes_read);
                return true;
            }
        }
    }

    serial_printf("File not found\n");
    return false;
}
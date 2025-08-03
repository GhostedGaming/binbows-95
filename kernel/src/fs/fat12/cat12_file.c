#include <fat12.h>

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
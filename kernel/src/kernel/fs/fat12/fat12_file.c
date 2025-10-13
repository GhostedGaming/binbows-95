#include <fat12.h>

// Updated fat12_write_file with size parameter
int fat12_write_file(uint8_t drive, const char* filename, const uint8_t* data, uint32_t size) {
    bpb_t12 bpb;

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
        return -2;  // Specific error code for no free clusters
    }
    
    int dir_entry = fat12_find_free_root_dir_entry(drive, &bpb);
    if (dir_entry == -1) {
        serial_printf("No free directory entry\n");
        return -3;  // Specific error code for no free directory entries
    }
    
    fat12_write_dir_entry(drive, &bpb, dir_entry, filename, first_cluster, size);
    fat12_write_clusters(drive, first_cluster, data, size);
    
    serial_printf("File written: %s size: %u clusters: %u\n", filename, size, clusters_needed);
    return 0;
}

// Updated fat12_read_file that returns allocated buffer
uint8_t* fat12_read_file(uint8_t drive, const char *filename, uint32_t *size_out) {
    bpb_t12 bpb;
    
    if (read_bpb(drive, &bpb) != 0) {
        serial_printf("Failed to read BPB\n");
        return NULL;
    }
    
    if (!validate_bpb(&bpb)) {
        serial_printf("Invalid BPB - filesystem may not be formatted properly\n");
        return NULL;
    }
    
    uint32_t root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
    uint32_t fat_size = bpb.fat_size_16;
    uint32_t root_dir_lba = bpb.reserved_sector_count + (bpb.num_fats * fat_size);
    
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

            if ((uint8_t)entry->name[0] == 0x00) {
                serial_printf("End of directory reached\n");
                return NULL;
            }
            
            if ((uint8_t)entry->name[0] == 0xE5) {
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
                uint32_t cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;
                uint32_t data_start_lba = root_dir_lba + root_dir_sectors;

                // Allocate buffer for file contents using kernel allocator
                uint8_t *buffer = (uint8_t*)kmalloc(file_size);
                if (!buffer) {
                    serial_printf("Failed to allocate memory for file (%u bytes)\n", file_size);
                    return NULL;
                }

                uint32_t bytes_read = 0;

                // Read FAT table
                uint8_t fat_table[512 * 12];
                if (ide_read_sectors(drive, fat_size, bpb.reserved_sector_count, fat_table) != 0) {
                    serial_printf("Failed to read FAT table\n");
                    kfree(buffer);
                    return NULL;
                }

                while (cluster >= 2 && cluster < FAT12_EOC && bytes_read < file_size) {
                    uint32_t lba = data_start_lba + (cluster - 2) * bpb.sectors_per_cluster;
                    uint32_t bytes_to_read = (file_size - bytes_read > cluster_size) ? 
                                           cluster_size : (file_size - bytes_read);
                    
                    uint8_t cluster_data[cluster_size];
                    if (ide_read_sectors(drive, bpb.sectors_per_cluster, lba, cluster_data) != 0) {
                        serial_printf("Failed to read cluster %u\n", cluster);
                        kfree(buffer);
                        return NULL;
                    }
                    
                    memcpy(buffer + bytes_read, cluster_data, bytes_to_read);
                    bytes_read += bytes_to_read;

                    // Get next cluster from FAT
                    cluster = fat12_read_entry(fat_table, cluster);
                    serial_printf("Next cluster: %u\n", cluster);
                }

                *size_out = bytes_read;
                serial_printf("Successfully read %u bytes\n", bytes_read);
                return buffer; // Caller must kfree() this buffer
            }
        }
    }

    serial_printf("File not found\n");
    return NULL;
}

// Optional: Keep the old function for backward compatibility
bool fat12_read_file_to_buffer(uint8_t drive, const char *filename, uint8_t *buffer, uint32_t *size_out) {
    uint8_t *file_data = fat12_read_file(drive, filename, size_out);
    if (!file_data) {
        return false;
    }
    
    memcpy(buffer, file_data, *size_out);
    kfree(file_data);
    return true;
}

void fat12_write_clusters(uint8_t drive, uint16_t first_cluster, const uint8_t *data, uint32_t size) {
    bpb_t12 bpb;
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
    
    while (remaining > 0 && current_cluster >= 2 && current_cluster < FAT12_EOC) {
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
            if (current_cluster < 2 || current_cluster >= FAT12_EOC) {
                serial_printf("Unexpected end of cluster chain while writing\n");
                break;
            }
        } else {
            // Mark last cluster as EOC
            fat12_write_entry(fat, current_cluster, 0xFFF);
            break;
        }
    }

    // Write updated FAT tables back to disk
    for (uint8_t fat_index = 0; fat_index < bpb.num_fats; fat_index++) {
        uint32_t fat_start_lba = bpb.reserved_sector_count + fat_index * bpb.fat_size_16;
        ide_write_sectors(drive, bpb.fat_size_16, fat_start_lba, fat);
    }
}

char *fat12_read_files(uint8_t drive) {
    static char file_list[4096];
    
    bpb_t12 bpb;
    if (read_bpb(drive, &bpb) != 0 || !validate_bpb(&bpb)) {
        serial_printf("Failed to read or validate BPB\n");
        strcpy(file_list, "Error: Failed to read BPB\n");
        return file_list;
    }
    
    uint32_t root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
    uint32_t fat_size = bpb.fat_size_16;
    uint32_t root_dir_lba = bpb.reserved_sector_count + (bpb.num_fats * fat_size);
    
    uint8_t sector[512];
    file_list[0] = '\0';
    size_t remaining = sizeof(file_list) - 1;
    size_t used = 0;
    
    for (uint32_t i = 0; i < root_dir_sectors; i++) {
        if (ide_read_sectors(drive, 1, root_dir_lba + i, sector) != 0) {
            serial_printf("Failed to read root directory sector %u\n", i);
            continue;
        }
        
        for (uint32_t j = 0; j < 512; j += 32) {
            fat12_dir_entry_t *entry = (fat12_dir_entry_t *)&sector[j];
            
            if (entry->name[0] == 0x00) {
                return file_list;
            }
            
            if (entry->name[0] == (char)0xE5 || 
                (entry->attr & ATTR_VOLUME_ID) || 
                (entry->attr & ATTR_DIRECTORY)) {
                continue;
            }
            
            char name[13] = {0};
            int name_idx = 0;
            
            int name_end = 8;
            while (name_end > 0 && entry->name[name_end - 1] == ' ') {
                name_end--;
            }
            for (int k = 0; k < name_end; k++) {
                name[name_idx++] = entry->name[k];
            }
            
            if (entry->name[8] != ' ') {
                name[name_idx++] = '.';
                int ext_end = 11;
                while (ext_end > 8 && entry->name[ext_end - 1] == ' ') {
                    ext_end--;
                }
                for (int k = 8; k < ext_end; k++) {
                    name[name_idx++] = entry->name[k];
                }
            }
            name[name_idx] = '\0';
            
            char temp[64];
            int len = 0;
            
            strcpy(temp, "Name: ");
            strcat(temp, name);
            strcat(temp, " Size: ");
            
            char size_str[16];
            uint32_t size = entry->file_size;
            int size_len = 0;
            
            if (size == 0) {
                size_str[size_len++] = '0';
            } else {
                char rev_str[16];
                int rev_len = 0;
                while (size > 0) {
                    rev_str[rev_len++] = '0' + (size % 10);
                    size /= 10;
                }
                for (int x = 0; x < rev_len; x++) {
                    size_str[size_len++] = rev_str[rev_len - 1 - x];
                }
            }
            size_str[size_len] = '\0';
            
            strcat(temp, size_str);
            strcat(temp, " bytes\n");
            
            len = strlen(temp);
            
            if (used + len >= remaining) {
                strcat(file_list, "[TRUNCATED - too many files]\n");
                return file_list;
            }
            
            strcat(file_list, temp);
            used += len;
            
            serial_printf("File: %s Size: %u bytes\n", name, entry->file_size);
        }
    }
    
    return file_list;
}
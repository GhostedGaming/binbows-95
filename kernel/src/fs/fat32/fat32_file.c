#include <fat32.h>

#define FAT32_EOC 0x0FFFFFF8

void fat32_write_clusters(uint8_t drive, uint32_t first_cluster, const uint8_t *data, uint32_t size) {
    bpb_t32 bpb;
    uint8_t fat[512 * 128];

    if (read_bpb(drive, &bpb) != 0 || !validate_bpb(&bpb)) {
        serial_printf("Invalid BPB in fat32_write_clusters\n");
        return;
    }

    if (ide_read_sectors(drive, bpb.fat_size_32, bpb.reserved_sector_count, fat) != 0) {
        serial_printf("Failed to read FAT in fat32_write_clusters\n");
        return;
    }

    uint32_t *fat32 = (uint32_t *)fat;
    uint32_t data_start_lba = bpb.reserved_sector_count + (bpb.num_fats * bpb.fat_size_32);
    uint32_t cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;

    uint32_t current_cluster = first_cluster;
    uint32_t remaining = size;
    const uint8_t *data_ptr = data;

    while (remaining > 0 && current_cluster >= 2 && current_cluster < FAT32_EOC) {
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
            uint32_t next_cluster = fat32[current_cluster] & 0x0FFFFFFF;
            if (next_cluster < 2 || next_cluster >= FAT32_EOC) {
                serial_printf("Unexpected end of cluster chain while writing\n");
                break;
            }
            current_cluster = next_cluster;
        } else {
            fat32[current_cluster] = FAT32_EOC;
            break;
        }
    }

    for (uint8_t fat_index = 0; fat_index < bpb.num_fats; fat_index++) {
        uint32_t fat_start_lba = bpb.reserved_sector_count + fat_index * bpb.fat_size_32;
        ide_write_sectors(drive, bpb.fat_size_32, fat_start_lba, fat);
    }
}

int fat32_write_file(uint8_t drive, const char* filename, const uint8_t* data, uint32_t size) {
    bpb_t32 bpb;

    if (read_bpb(drive, &bpb) != 0 || !validate_bpb(&bpb)) {
        serial_printf("Invalid BPB in fat32_write_file\n");
        return -1;
    }

    uint32_t cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;
    uint32_t clusters_needed = (size + cluster_size - 1) / cluster_size;
    if (clusters_needed == 0) clusters_needed = 1;

    uint32_t first_cluster = fat32_alloc_clusters(drive, clusters_needed);
    if (first_cluster == 0) {
        serial_printf("No free clusters available\n");
        return -1;
    }

    int dir_entry = fat32_find_free_dir_entry(drive, &bpb, bpb.root_cluster);
    if (dir_entry == -1) {
        serial_printf("No free directory entry\n");
        return -1;
    }

    fat32_write_dir_entry(drive, &bpb, dir_entry, filename, first_cluster, size);
    fat32_write_clusters(drive, first_cluster, data, size);

    serial_printf("FAT32: File written: %s size: %u clusters: %u\n", filename, size, clusters_needed);
    return 0;
}

bool fat32_read_file(uint8_t drive, const char *filename, uint8_t *buffer, uint32_t *size_out) {
    bpb_t32 bpb;

    if (read_bpb(drive, &bpb) != 0 || !validate_bpb(&bpb)) {
        serial_printf("Invalid BPB in fat32_read_file\n");
        return false;
    }

    uint32_t data_start_lba = bpb.reserved_sector_count + (bpb.num_fats * bpb.fat_size_32);
    char fat_filename[12];
    fat16_format_filename_for_compare(filename, fat_filename);

    uint8_t sector[512];
    for (uint32_t i = 0; i < bpb.root_cluster; i++) {
        if (ide_read_sectors(drive, 1, data_start_lba + i, sector) != 0) {
            continue;
        }

        for (uint32_t j = 0; j < 512; j += 32) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t *)&sector[j];

            if (entry->name[0] == 0x00) return false;
            if (entry->name[0] == 0xE5) continue;
            if ((entry->attr & ATTR_DIRECTORY) || (entry->attr & ATTR_VOLUME_ID)) continue;
            if (memcmp(entry->name, fat_filename, 11) != 0) continue;

            uint32_t cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
            uint32_t file_size = entry->file_size;
            uint32_t bytes_read = 0;
            uint32_t cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;

            uint8_t fat_table[512 * 128];
            if (ide_read_sectors(drive, bpb.fat_size_32, bpb.reserved_sector_count, fat_table) != 0) return false;

            uint32_t *fat32 = (uint32_t *)fat_table;

            while (cluster >= 2 && cluster < FAT32_EOC && bytes_read < file_size) {
                uint32_t lba = data_start_lba + (cluster - 2) * bpb.sectors_per_cluster;
                uint32_t bytes_to_read = (file_size - bytes_read > cluster_size) ? cluster_size : (file_size - bytes_read);

                uint8_t cluster_data[cluster_size];
                if (ide_read_sectors(drive, bpb.sectors_per_cluster, lba, cluster_data) != 0) break;

                memcpy(buffer + bytes_read, cluster_data, bytes_to_read);
                bytes_read += bytes_to_read;

                cluster = fat32[cluster] & 0x0FFFFFFF;
            }

            *size_out = bytes_read;
            return true;
        }
    }

    return false;
}

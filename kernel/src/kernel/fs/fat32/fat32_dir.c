#include <fat32.h>

int fat32_find_free_dir_entry(uint8_t drive, bpb_t32* bpb, uint32_t dir_cluster) {
    uint8_t sector[512];
    uint32_t sectors_per_cluster = bpb->sectors_per_cluster;
    uint32_t bytes_per_sector = bpb->bytes_per_sector;
    uint32_t entries_per_sector = bytes_per_sector / 32;

    while (dir_cluster >= 2 && dir_cluster < FAT32_EOC) {
        for (uint32_t i = 0; i < sectors_per_cluster; i++) {
            uint32_t sector_lba = cluster_to_lba(bpb, dir_cluster) + i;

            if (ide_read_sectors(drive, 1, sector_lba, sector) != 0)
                continue;

            for (uint32_t offset = 0; offset < bytes_per_sector; offset += 32) {
                if (sector[offset] == 0x00 || sector[offset] == 0xE5) {
                    uint32_t dir_index = (i * entries_per_sector) + (offset / 32);
                    return dir_index;
                }
            }
        }

        // Read FAT to follow next cluster in chain
        uint8_t fat_buffer[512 * 4]; // enough for large FAT chunk
        if (ide_read_sectors(drive, bpb->fat_size_32, bpb->reserved_sector_count, fat_buffer) != 0)
            return -1;

        dir_cluster = fat32_read_entry(fat_buffer, dir_cluster);
    }

    return -1;
}

void fat32_write_dir_entry(uint8_t drive, bpb_t32* bpb, uint32_t parent_cluster, const char* filename, uint32_t first_cluster, uint32_t size) {
    uint8_t sector[512];
    uint32_t bytes_per_sector = bpb->bytes_per_sector;
    uint32_t entries_per_sector = bytes_per_sector / 32;
    uint32_t sectors_per_cluster = bpb->sectors_per_cluster;

    int entry_index = fat32_find_free_dir_entry(drive, bpb, parent_cluster);
    if (entry_index < 0) {
        serial_printf("No free directory entry found\n");
        return;
    }

    uint32_t cluster_offset = entry_index / entries_per_sector / sectors_per_cluster;
    uint32_t sector_offset = (entry_index / entries_per_sector) % sectors_per_cluster;
    uint32_t entry_offset = (entry_index % entries_per_sector) * 32;

    uint32_t cluster = parent_cluster;
    for (uint32_t i = 0; i < cluster_offset; i++) {
        uint8_t fat_buf[512 * 4];
        if (ide_read_sectors(drive, bpb->fat_size_32, bpb->reserved_sector_count, fat_buf) != 0)
            return;
        cluster = fat32_read_entry(fat_buf, cluster);
        if (cluster >= FAT32_EOC) {
            serial_printf("Reached end of cluster chain too early\n");
            return;
        }
    }

    uint32_t sector_lba = cluster_to_lba(bpb, cluster) + sector_offset;
    if (ide_read_sectors(drive, 1, sector_lba, sector) != 0) {
        serial_printf("Failed to read directory sector\n");
        return;
    }

    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector + entry_offset);
    memset(entry, 0, sizeof(fat32_dir_entry_t));

    fat32_format_filename(filename, entry->name);
    entry->attr = ATTR_ARCHIVE;
    entry->first_cluster_low = (uint16_t)(first_cluster & 0xFFFF);
    entry->first_cluster_high = (uint16_t)((first_cluster >> 16) & 0xFFFF);
    entry->file_size = size;

    if (ide_write_sectors(drive, 1, sector_lba, sector) != 0) {
        serial_printf("Failed to write directory entry\n");
    }
}

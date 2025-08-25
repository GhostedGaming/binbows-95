#include <fat32.h>

uint32_t fat32_alloc_clusters(uint8_t drive, uint32_t count) {
    bpb_t32 bpb;
    uint8_t fat[512 * 128];  // Support up to 128 sectors of FAT (64 KB)

    if (read_bpb32(drive, &bpb) != 0) {
        serial_printf("Failed to read BPB in fat32_alloc_clusters\n");
        return 0;
    }

    if (!validate_bpb32(&bpb)) {
        serial_printf("Invalid BPB in fat32_alloc_clusters\n");
        return 0;
    }

    if (ide_read_sectors(drive, bpb.fat_size_32, bpb.reserved_sector_count, fat) != 0) {
        serial_printf("Failed to read FAT in fat32_alloc_clusters\n");
        return 0;
    }

    uint32_t fat_entries = (bpb.fat_size_32 * bpb.bytes_per_sector) / 4;
    uint32_t max_clusters = fat_entries;

    if (max_clusters < 3 || max_clusters > 0x0FFFFFF6) {
        serial_printf("Error: Invalid cluster count\n");
        return 0;
    }

    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;
    uint32_t allocated = 0;

    for (uint32_t cluster = 2; cluster < max_clusters && allocated < count; cluster++) {
        if (fat32_read_entry(fat, cluster) == FAT32_FREE_CLUSTER) {
            if (first_cluster == 0)
                first_cluster = cluster;
            else
                fat32_write_entry(fat, prev_cluster, cluster);

            prev_cluster = cluster;
            allocated++;
        }
    }

    if (allocated == count && prev_cluster != 0) {
        fat32_write_entry(fat, prev_cluster, FAT32_EOC);

        fat32_write_fat(drive, bpb.num_fats, bpb.fat_size_32, fat, bpb.reserved_sector_count);
        return first_cluster;
    }

    serial_printf("Failed to allocate %u clusters (only allocated %u)\n", count, allocated);
    return 0;
}

uint32_t fat32_read_entry(const uint8_t* fat, uint32_t cluster) {
    if (cluster < 2) return FAT32_EOC;

    const uint32_t* fat32 = (const uint32_t*)fat;
    return fat32[cluster] & 0x0FFFFFFF;
}

void fat32_write_entry(uint8_t* fat, uint32_t cluster, uint32_t value) {
    if (cluster < 2) return;

    uint32_t* fat32 = (uint32_t*)fat;
    fat32[cluster] = (fat32[cluster] & 0xF0000000) | (value & 0x0FFFFFFF);
}

void fat32_write_fat(uint8_t drive, uint8_t num_fats, uint32_t fat_size_32, const uint8_t* fat_data, uint16_t reserved_sector_count) {
    for (uint8_t i = 0; i < num_fats; i++) {
        uint32_t fat_start = reserved_sector_count + i * fat_size_32;
        for (uint32_t s = 0; s < fat_size_32; s++) {
            ide_write_sectors(drive, 1, fat_start + s, fat_data + s * SECTOR_SIZE);
        }
    }
}
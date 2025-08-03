#include <fat16.h>

uint16_t fat16_alloc_clusters(uint8_t drive, uint16_t count) {
    bpb_t bpb;
    uint8_t fat[512 * 12];  // Support up to 12 sectors of FAT
    
    if (read_bpb(drive, &bpb) != 0) {
        serial_printf("Failed to read BPB in fat16_alloc_clusters\n");
        return 0;
    }
    
    if (!validate_bpb(&bpb)) {
        serial_printf("Invalid BPB in fat16_alloc_clusters\n");
        return 0;
    }
    
    // Read all FAT sectors
    if (ide_read_sectors(drive, bpb.fat_size_16, bpb.reserved_sector_count, fat) != 0) {
        serial_printf("Failed to read FAT in fat16_alloc_clusters\n");
        return 0;
    }
    
    uint32_t fat_entries = (bpb.fat_size_16 * bpb.bytes_per_sector) / 2;
    uint16_t max_clusters = fat_entries;
    
    // FAT16 cluster range is 2 to 65525
    if (max_clusters > 65525) max_clusters = 65525;
    
    if (max_clusters < 3) {
        serial_printf("Error: Too few clusters available\n");
        return 0;
    }
    
    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;
    uint16_t allocated = 0;
    
    // Find and allocate free clusters
    for (uint16_t cluster = 2; cluster < max_clusters && allocated < count; cluster++) {
        if (fat16_read_entry(fat, cluster) == 0x0000) {
            if (first_cluster == 0) {
                first_cluster = cluster;
            } else {
                fat16_write_entry(fat, prev_cluster, cluster);
            }
            prev_cluster = cluster;
            allocated++;
        }
    }
    
    if (allocated == count && prev_cluster != 0) {
        // Mark last cluster as end-of-chain
        fat16_write_entry(fat, prev_cluster, 0xFFFF);
        
        // Write FAT back to disk
        fat16_write_fat(drive, bpb.num_fats, bpb.fat_size_16, fat, bpb.reserved_sector_count);
        return first_cluster;
    }
    
    serial_printf("Failed to allocate %u clusters (only allocated %u)\n", count, allocated);
    return 0;
}

uint16_t fat16_read_entry(const uint8_t* fat, uint16_t cluster) {
    if (cluster < 2) return 0xFFFF;  // Invalid cluster
    
    const uint16_t* fat16 = (const uint16_t*)fat;
    return fat16[cluster];
}

void fat16_write_entry(uint8_t* fat, uint16_t cluster, uint16_t value) {
    if (cluster < 2) return;  // Invalid cluster
    
    uint16_t* fat16 = (uint16_t*)fat;
    fat16[cluster] = value;
}

int fat16_find_free_cluster(uint8_t* fat, uint16_t max_clusters) {
    for (uint16_t cluster = 2; cluster < max_clusters; cluster++) {
        if (fat16_read_entry(fat, cluster) == 0x0000) {
            return cluster;
        }
    }
    return -1;
}

void fat16_write_fat(uint8_t drive, uint8_t num_fats, uint16_t fat_size_16, const uint8_t* fat_data, uint8_t reserved_sector_count) {
    // Write all FAT copies
    for (uint8_t i = 0; i < num_fats; i++) {
        uint32_t fat_start = reserved_sector_count + i * fat_size_16;
        for (uint16_t s = 0; s < fat_size_16; s++) {
            ide_write_sectors(drive, 1, fat_start + s, fat_data + s * 512);
        }
    }
}
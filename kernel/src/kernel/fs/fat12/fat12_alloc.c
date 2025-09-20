#include <fat12.h>

uint16_t fat12_alloc_clusters(uint8_t drive, uint16_t count) {
    bpb_t12 bpb;
    uint8_t fat[512 * 12];
    
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
    if (cluster < 2) return 0xFFF;
    
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
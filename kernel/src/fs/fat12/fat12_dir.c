#include <fat12.h>

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
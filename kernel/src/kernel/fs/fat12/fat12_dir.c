#include <fat12.h>

int fat12_find_free_root_dir_entry(uint8_t drive, bpb_t12* bpb) {
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

void fat12_write_dir_entry(uint8_t drive, bpb_t12* bpb, int index, const char* filename, uint16_t first_cluster, uint32_t size) {
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

void fat12_write_subdir(uint8_t drive, bpb_t12* bpb, int parent_dir_index, const char* dirname, uint16_t first_cluster) {
    uint32_t lba = bpb->reserved_sector_count + bpb->num_fats * bpb->fat_size_16;
    uint32_t entries_per_sector = bpb->bytes_per_sector / 32;
    uint32_t sector_index = parent_dir_index / entries_per_sector;
    uint32_t entry_offset = (parent_dir_index % entries_per_sector) * 32;

    uint8_t sector[512];
    if (ide_read_sectors(drive, 1, lba + sector_index, sector) != 0) {
        serial_printf("Failed to read parent directory sector\n");
        return;
    }

    fat12_dir_entry_t* entry = (fat12_dir_entry_t*)(sector + entry_offset);
    memset(entry, 0, 32);

    fat12_format_filename(dirname, entry->name);
    entry->attr = ATTR_DIRECTORY;
    entry->first_cluster_low = first_cluster;
    entry->first_cluster_high = 0;
    entry->file_size = 0;

    if (ide_write_sectors(drive, 1, lba + sector_index, sector) != 0) {
        serial_printf("Failed to write parent directory entry\n");
        return;
    }

    uint32_t root_dir_sectors = ((bpb->root_entry_count * 32) + (bpb->bytes_per_sector - 1)) / bpb->bytes_per_sector;
    uint32_t data_start_lba = bpb->reserved_sector_count + (bpb->num_fats * bpb->fat_size_16) + root_dir_sectors;
    uint32_t cluster_lba = data_start_lba + (first_cluster - 2) * bpb->sectors_per_cluster;

    uint8_t cluster_data[512];
    memset(cluster_data, 0, sizeof(cluster_data));

    fat12_dir_entry_t* dot_entry = (fat12_dir_entry_t*)cluster_data;
    memset(dot_entry->name, ' ', 11);
    dot_entry->name[0] = '.';
    dot_entry->attr = ATTR_DIRECTORY;
    dot_entry->first_cluster_low = first_cluster;
    dot_entry->first_cluster_high = 0;
    dot_entry->file_size = 0;

    fat12_dir_entry_t* dotdot_entry = (fat12_dir_entry_t*)(cluster_data + 32);
    memset(dotdot_entry->name, ' ', 11);
    dotdot_entry->name[0] = '.';
    dotdot_entry->name[1] = '.';
    dotdot_entry->attr = ATTR_DIRECTORY;
    dotdot_entry->first_cluster_low = 0;  // 0 means root directory
    dotdot_entry->first_cluster_high = 0;
    dotdot_entry->file_size = 0;

    if (ide_write_sectors(drive, bpb->sectors_per_cluster, cluster_lba, cluster_data) != 0) {
        serial_printf("Failed to write subdirectory cluster\n");
        return;
    }

    serial_printf("Subdirectory '%s' created at cluster %u\n", dirname, first_cluster);
}
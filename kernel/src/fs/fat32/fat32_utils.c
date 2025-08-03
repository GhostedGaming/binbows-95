#include <fat32.h>

// Format a filename to 8.3 FAT format (FAT32 uses the same short name format)
void fat32_format_filename(const char* filename, char* fat_name) {
    memset(fat_name, ' ', 11);
    fat_name[11] = '\0';  // Null-terminate for debugging/logging (not stored on disk)

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
            fat_name[i] -= 'a' - 'A';
        }
    }
}

// For comparing or searching names, uses same formatting
void fat32_format_filename_for_compare(const char* filename, char* fat_name) {
    fat32_format_filename(filename, fat_name);
}

// Validate that the loaded BPB is a proper FAT32 structure
bool validate_bpb32(bpb_t *bpb) {
    if (bpb->bytes_per_sector != 512) {
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
    if (bpb->fat_size_32 == 0) {
        serial_printf("Invalid fat_size_32: %u\n", bpb->fat_size_32);
        return false;
    }
    if (bpb->root_cluster < 2 || bpb->root_cluster >= FAT32_EOC) {
        serial_printf("Invalid root_cluster: %u\n", bpb->root_cluster);
        return false;
    }
    return true;
}

// Read BPB from sector 0 and cache it
int read_bpb32(uint8_t drive, bpb_t *bpb) {
    if (bpb_cached && cached_drive == drive) {
        memcpy(bpb, &cached_bpb, sizeof(bpb_t));
        return 0;
    }

    uint8_t boot_sector[512];
    if (ide_read_sectors(drive, 1, 0, boot_sector) != 0) {
        serial_printf("Failed to read boot sector\n");
        return -1;
    }

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

uint32_t cluster_to_lba(bpb_t* bpb, uint32_t cluster) {
    uint32_t first_data_sector =
        bpb->reserved_sector_count +
        (bpb->num_fats * bpb->fat_size_32);

    return first_data_sector + ((cluster - 2) * bpb->sectors_per_cluster);
}
#include <fat16.h>

void fat16_format_filename16(const char* filename, char* fat_name) {
    memset(fat_name, ' ', 11);
    fat_name[11] = '\0';  // Null terminate for debugging
    
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
            fat_name[i] = fat_name[i] - 'a' + 'A';
        }
    }
}

void fat16_format_filename_for_compare16(const char* filename, char* fat_name) {
    fat16_format_filename(filename, fat_name);
}

bool validate_bpb16(bpb_t16 *bpb) {
    if (bpb->bytes_per_sector == 0 || bpb->bytes_per_sector != 512) {
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
    if (bpb->fat_size_16 == 0) {
        serial_printf("Invalid fat_size_16: %u\n", bpb->fat_size_16);
        return false;
    }
    if (bpb->root_entry_count == 0) {
        serial_printf("Invalid root_entry_count: %u\n", bpb->root_entry_count);
        return false;
    }
    return true;
}

int read_bpb16(uint8_t drive, bpb_t16 *bpb) {
    // Use cached BPB if available for the same drive
    if (bpb_cached16 && cached_drive16 == drive) {
        memcpy(bpb, &cached_bpb16, sizeof(bpb_t16));
        return 0;
    }
    
    uint8_t boot_sector[512];
    if (ide_read_sectors(drive, 1, 0, boot_sector) != 0) {
        return -1;
    }
    
    // Verify boot signature
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        serial_printf("Invalid boot signature: 0x%02X%02X\n", boot_sector[511], boot_sector[510]);
        return -1;
    }
    
    memcpy(bpb, boot_sector + 11, sizeof(bpb_t16));
    
    // Cache the BPB
    memcpy(&cached_bpb16, bpb, sizeof(bpb_t16));
    bpb_cached16 = true;
    cached_drive16 = drive;
    
    return 0;
}
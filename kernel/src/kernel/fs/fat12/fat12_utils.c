#include <fat12.h>

void fat12_format_filename(const char* filename, char* fat_name) {
    memset(fat_name, ' ', 11);
    fat_name[11] = '\0';
    
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

void format_filename_for_compare(const char* filename, char* fat_name) {
    fat12_format_filename(filename, fat_name);
}

bool validate_bpb(bpb_t12 *bpb) {
    if (bpb->bytes_per_sector == 0 || bpb->bytes_per_sector != 512) {
        serial_printf("Invalid bytes_per_sector: %u\n", bpb->bytes_per_sector);
        return false;
    }
    // Allow multiple sectors per cluster
    if (bpb->sectors_per_cluster == 0 || bpb->sectors_per_cluster > 128) {
        serial_printf("Invalid sectors_per_cluster: %u\n", bpb->sectors_per_cluster);
        return false;
    }
    // Allow multiple reserved sectors  
    if (bpb->reserved_sector_count == 0) {
        serial_printf("Invalid reserved_sector_count: %u\n", bpb->reserved_sector_count);
        return false;
    }
    if (bpb->num_fats == 0 || bpb->num_fats > 2) {
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

int read_bpb(uint8_t drive, bpb_t12 *bpb) {
    if (bpb_cached12 && cached_drive12 == drive) {
        memcpy(bpb, &cached_bpb12, sizeof(bpb_t12));
        return 0;
    }
    
    uint8_t boot_sector[512];
    if (ide_read_sectors(drive, 1, 0, boot_sector) != 0) {
        return -1;
    }
    
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        serial_printf("Invalid boot signature: 0x%02X%02X\n", boot_sector[511], boot_sector[510]);
        return -1;
    }
    
    memcpy(bpb, boot_sector + 11, sizeof(bpb_t12));
    
    memcpy(&cached_bpb12, bpb, sizeof(bpb_t12));
    bpb_cached12 = true;
    cached_drive12 = drive;
    
    return 0;
}
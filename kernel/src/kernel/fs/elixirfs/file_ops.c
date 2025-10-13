#include <elixir.h>
#include <mem.h>
#include <sata.h>
#include <util.h>
#include <serial.h>

static uint32_t hash_name(const char *name) {
    uint32_t hash = hash_string(name);
    return hash % MAX_FILES;
}

struct file_table* create_file_table() {
    struct file_table* ft = kmalloc(sizeof(struct file_table));
    
    if (!ft) {
        return NULL;
    }

    memset(ft, 0, sizeof(struct file_table));

    ft->magic = FILE_TABLE_MAGIC;
    ft->max_entries = MAX_FILES;
    ft->num_entries = 0;
    ft->reserved = 5;

    return ft;
}

void destroy_file_table(struct file_table* ft) {
    if (ft) {
        kfree(ft);
    }
}

void destroy_file(struct file_entry* fe) {
    if (fe) {
        kfree(fe);
    }
}

int new_entry(struct file_table* ft, struct file_entry* fe) {
    if (!ft || !fe) {
        return -1;
    }

    if (ft->num_entries >= ft->max_entries) {
        return -2;
    }

    uint32_t hash = hash_name(fe->name);

    for (uint32_t i = 0; i < ft->max_entries; i++) {
        uint32_t index = (hash + i) % ft->max_entries;
        struct file_entry* slot = &ft->entries[index];

        if (slot->size_bytes == 0) {
            memcpy(slot, fe, sizeof(struct file_entry));
            ft->num_entries++;
            return 0;
        }

        if (strcmp(slot->name, fe->name) == 0) {
            return -3;
        }
    }

    return -4;
}

struct file_entry* find_file_entry(struct file_table* ft, const char* name) {
    serial_printf("find_file_entry: Searching for '%s'\n", name);
    
    uint32_t hash = hash_name(name);
    serial_printf("find_file_entry: Hash = %u\n", hash);
    
    for (uint32_t i = 0; i < ft->max_entries; i++) {
        uint32_t index = (hash + i) % ft->max_entries;
        struct file_entry* fe = &ft->entries[index];
        
        if (fe->size_bytes == 0) {
            serial_printf("find_file_entry: Found empty slot at index %u\n", index);
            return NULL;
        }
        
        serial_printf("find_file_entry: Checking entry %u: '%s'\n", index, fe->name);
        
        if (strcmp(fe->name, name) == 0) {
            serial_printf("find_file_entry: Found match at index %u\n", index);
            return fe;
        }
    }
    
    serial_printf("find_file_entry: File not found after full scan\n");
    return NULL;
}

struct file_table* load_file_table(HBA_PORT *port) {
    struct file_table* ft = kmalloc(sizeof(struct file_table));

    if (!ft) {
        return NULL;
    }

    uint64_t file_table_sectors = (sizeof(struct file_table) + 511) / 512;

    if (!read_sectors(port, 8, 0, file_table_sectors, ft)) {
        kfree(ft);
        return NULL;
    }

    if (ft->magic != FILE_TABLE_MAGIC) {
        kfree(ft);
        return NULL;
    }

    return ft;
}

int save_file_table(HBA_PORT *port, struct file_table* ft) {
    if (!ft) {
        return -1;
    }

    uint64_t file_table_sectors = (sizeof(struct file_table) + 511) / 512;

    if (!write_sectors(port, 8, 0, file_table_sectors, ft)) {
        return -1;
    }

    return 0;
}

int create_file(char* name, HBA_PORT *port, void* data, size_t data_size) {
    serial_printf("create_file: Starting file creation for '%s'\n", name);
    
    uint16_t block_size = 4096;
    uint8_t sectors_per_block = 8;

    serial_printf("create_file: Allocating file entry\n");
    struct file_entry* fe = kmalloc(sizeof(struct file_entry));
    if (!fe) {
        serial_printf("create_file: Failed to allocate file entry\n");
        return -1;
    }

    memset(fe, 0, sizeof(struct file_entry));
    serial_printf("create_file: Copying name\n");
    
    int i;
    for (i = 0; i < MAX_FILENAME - 1 && name[i]; i++) {
        fe->name[i] = name[i];
    }
    fe->name[i] = '\0';

    serial_printf("create_file: Loading superblock\n");
    struct super_block* sb = kmalloc(sizeof(struct super_block));
    if (!sb) {
        serial_printf("create_file: Failed to allocate superblock\n");
        kfree(fe);
        return -2;
    }

    serial_printf("create_file: Reading superblock from disk\n");
    if (!read_sectors(port, 0, 0, sectors_per_block, sb)) {
        serial_printf("create_file: Failed to read superblock\n");
        kfree(sb);
        kfree(fe);
        return -2;
    }

    serial_printf("create_file: Verifying superblock magic\n");
    if (sb->magic != SUPER_BLOCK_MAGIC) {
        serial_printf("create_file: Invalid superblock magic: 0x%x\n", sb->magic);
        kfree(sb);
        kfree(fe);
        return -2;
    }

    serial_printf("create_file: Searching for free block (start=%llu, end=%llu)\n", 
                  sb->data_start_block, sb->total_blocks);
    
    fe->start_block = 0;
    uint64_t search_start = sb->data_start_block;
    uint64_t search_end = sb->total_blocks;

    for (uint64_t i = search_start; i < search_end; i++) {
        uint16_t buffer[256] = {0};
        uint64_t sector = i * sectors_per_block;
        if (!read_sectors(port, (uint32_t)(sector & 0xFFFFFFFF), (uint32_t)(sector >> 32), sectors_per_block, buffer)) {
            continue;
        }

        int is_empty = 1;
        for (int j = 0; j < 256; j++) {
            if (buffer[j] != 0) {
                is_empty = 0;
                break;
            }
        }

        if (is_empty) {
            fe->start_block = i;
            serial_printf("create_file: Found free block at %llu\n", i);
            break;
        }
    }

    if (fe->start_block == 0) {
        serial_printf("create_file: No free block found\n");
        kfree(sb);
        kfree(fe);
        return -2;
    }

    fe->size_bytes = data_size;
    fe->size_blocks = (fe->size_bytes + block_size - 1) / block_size;

    serial_printf("create_file: Writing file data (size=%llu bytes, blocks=%u)\n", 
                  fe->size_bytes, fe->size_blocks);
    
    uint32_t sectors_needed = fe->size_blocks * sectors_per_block;
    uint64_t start_sector = fe->start_block * sectors_per_block;

    if (!write_sectors(port, (uint32_t)(start_sector & 0xFFFFFFFF), (uint32_t)(start_sector >> 32), sectors_needed, data)) {
        serial_printf("create_file: Failed to write file data\n");
        kfree(sb);
        kfree(fe);
        return -3;
    }

    serial_printf("create_file: Loading file table\n");
    struct file_table* ft = load_file_table(port);
    if (!ft) {
        serial_printf("create_file: Failed to load file table\n");
        kfree(sb);
        kfree(fe);
        return -4;
    }

    serial_printf("create_file: Adding entry to file table\n");
    int result = new_entry(ft, fe);
    if (result != 0) {
        serial_printf("create_file: Failed to add entry: %d\n", result);
        kfree(ft);
        kfree(sb);
        kfree(fe);
        return -5;
    }

    serial_printf("create_file: Saving file table\n");
    if (save_file_table(port, ft) != 0) {
        serial_printf("create_file: Failed to save file table\n");
        kfree(ft);
        kfree(sb);
        kfree(fe);
        return -6;
    }

    kfree(ft);
    kfree(sb);
    kfree(fe);
    return 0;
}

void* read_file(char* name, HBA_PORT *port, size_t* out_size) {
    serial_printf("read_file: Starting file read for '%s'\n", name);
    
    serial_printf("read_file: Loading file table\n");
    struct file_table* ft = load_file_table(port);
    if (!ft) {
        serial_printf("read_file: Failed to load file table\n");
        if (out_size) *out_size = 0;
        return NULL;
    }
    
    serial_printf("read_file: Finding file entry\n");
    struct file_entry* fe = find_file_entry(ft, name);
    if (!fe) {
        serial_printf("read_file: File not found\n");
        kfree(ft);
        if (out_size) *out_size = 0;
        return NULL;
    }
    
    serial_printf("read_file: Found file - start_block=%u, size_bytes=%llu, size_blocks=%u\n",
                  fe->start_block, fe->size_bytes, fe->size_blocks);
    
    serial_printf("read_file: Allocating buffer for %llu bytes\n", fe->size_bytes);
    void* file_data = kmalloc(fe->size_bytes);
    if (!file_data) {
        serial_printf("read_file: Failed to allocate buffer\n");
        kfree(ft);
        if (out_size) *out_size = 0;
        return NULL;
    }
    
    uint32_t sectors_to_read = fe->size_blocks * 8;
    uint64_t start_sector = fe->start_block * 8;
    
    serial_printf("read_file: Reading %u sectors from sector %llu\n", sectors_to_read, start_sector);
    
    if (!read_sectors(port, (uint32_t)(start_sector & 0xFFFFFFFF), (uint32_t)(start_sector >> 32), sectors_to_read, file_data)) {
        serial_printf("read_file: Failed to read file data\n");
        kfree(file_data);
        kfree(ft);
        if (out_size) *out_size = 0;
        return NULL;
    }
    
    serial_printf("read_file: Successfully read file\n");
    
    if (out_size) {
        *out_size = fe->size_bytes;
    }
    
    kfree(ft);
    serial_printf("read_file: Returning data\n");
    return file_data;
}
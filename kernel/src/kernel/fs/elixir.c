#include <elixir.h>
#include <mem.h>
#include <sata.h>
#include <util.h>
#include <serial.h>

#define SECTORS_PER_BLOCK (ELIXIR_BLOCK_SIZE / 512)

static bool read_blocks(HBA_PORT* port, void* buffer, uint64_t start_block, uint64_t num_blocks) {
    return ahci_read(port, start_block * SECTORS_PER_BLOCK, num_blocks * SECTORS_PER_BLOCK, buffer);
}

static bool write_blocks(HBA_PORT* port, const void* buffer, uint64_t start_block, uint64_t num_blocks) {
    return ahci_write(port, start_block * SECTORS_PER_BLOCK, num_blocks * SECTORS_PER_BLOCK, buffer);
}

static void set_bitmap_bit(uint8_t* bitmap, uint64_t block) {
    bitmap[block / 8] |= (1 << (block % 8));
}

static void clear_bitmap_bit(uint8_t* bitmap, uint64_t block) {
    bitmap[block / 8] &= ~(1 << (block % 8));
}

static bool is_bitmap_bit_set(const uint8_t* bitmap, uint64_t block) {
    return (bitmap[block / 8] & (1 << (block % 8))) != 0;
}

static int64_t find_free_block(elixir_fs_t* fs) {
    for (uint64_t i = fs->sb.data_start_block; i < fs->sb.total_blocks; ++i) {
        if (!is_bitmap_bit_set(fs->free_block_bitmap, i)) {
            return i;
        }
    }
    return -1;
}

// Helper to get a file entry from a path
// Returns the file entry and optionally the parent directory's file table and its block
static elixir_file_entry_t* elixir_get_file_entry_from_path(
    elixir_fs_t* fs, const char* path, elixir_file_table_t** out_parent_ft, uint64_t* out_parent_ft_block) {

    if (path == NULL || path[0] != '/') return NULL; // Must be an absolute path

    char path_copy[ELIXIR_MAX_FILENAME * ELIXIR_MAX_FILES]; // Max path length
    strcpy(path_copy, path);

    char* token = strtok(path_copy, "/");
    elixir_file_table_t* current_ft = kmalloc(ELIXIR_BLOCK_SIZE);
    if (!current_ft) return NULL;

    uint64_t current_ft_block = fs->sb.file_table_start_block;

    if (!read_blocks(fs->port, current_ft, current_ft_block, 1)) {
        kfree(current_ft);
        return NULL;
    }

    elixir_file_entry_t* found_entry = NULL;
    elixir_file_table_t* parent_ft = NULL;
    uint64_t parent_ft_block = 0;

    while (token != NULL) {
        found_entry = NULL;
        for (uint32_t i = 0; i < current_ft->num_entries; ++i) {
            if (strcmp(current_ft->entries[i].name, token) == 0) {
                found_entry = &current_ft->entries[i];
                break;
            }
        }

        if (found_entry == NULL) {
            kfree(current_ft);
            return NULL; // Path component not found
        }

        char* next_token = strtok(NULL, "/");
        if (next_token == NULL) {
            // This is the final component
            if (out_parent_ft) {
                parent_ft = kmalloc(ELIXIR_BLOCK_SIZE);
                if (!parent_ft) { kfree(current_ft); return NULL; }
                memcpy(parent_ft, current_ft, ELIXIR_BLOCK_SIZE);
                *out_parent_ft = parent_ft;
                *out_parent_ft_block = current_ft_block;
            }
            // We need to return a copy of the entry, as current_ft will be freed
            elixir_file_entry_t* entry_copy = kmalloc(sizeof(elixir_file_entry_t));
            if (!entry_copy) { kfree(current_ft); if (parent_ft) kfree(parent_ft); return NULL; }
            memcpy(entry_copy, found_entry, sizeof(elixir_file_entry_t));
            kfree(current_ft);
            return entry_copy;
        } else {
            // Not the final component, must be a directory
            if (found_entry->type != ELIXIR_FILE_TYPE_DIRECTORY) {
                kfree(current_ft);
                return NULL; // Not a directory
            }
            parent_ft_block = current_ft_block;
            current_ft_block = found_entry->start_block;
            if (!read_blocks(fs->port, current_ft, current_ft_block, 1)) {
                kfree(current_ft);
                return NULL;
            }
        }
        token = next_token;
    }

    kfree(current_ft);
    return NULL; // Should not reach here for valid paths
}

int elixir_format(HBA_PORT* port, uint64_t device_size_bytes) {
    elixir_superblock_t sb;
    memset(&sb, 0, sizeof(elixir_superblock_t));

    sb.magic = ELIXIR_SUPER_BLOCK_MAGIC;
    sb.version = 1;
    sb.total_blocks = device_size_bytes / ELIXIR_BLOCK_SIZE;

    sb.bitmap_start_block = 1;
    sb.bitmap_size_blocks = (sb.total_blocks + 8 - 1) / 8 / ELIXIR_BLOCK_SIZE;
    if (sb.bitmap_size_blocks == 0) sb.bitmap_size_blocks = 1;

    sb.file_table_start_block = sb.bitmap_start_block + sb.bitmap_size_blocks;
    sb.file_table_size_blocks = (sizeof(elixir_file_table_t) + ELIXIR_BLOCK_SIZE - 1) / ELIXIR_BLOCK_SIZE;

    sb.data_start_block = sb.file_table_start_block + sb.file_table_size_blocks;
    sb.free_blocks = sb.total_blocks - sb.data_start_block;

    void* block_buffer = kmalloc(ELIXIR_BLOCK_SIZE);
    if (!block_buffer) return -1;

    memset(block_buffer, 0, ELIXIR_BLOCK_SIZE);
    memcpy(block_buffer, &sb, sizeof(elixir_superblock_t));
    if (!write_blocks(port, block_buffer, 0, 1)) {
        kfree(block_buffer);
        return -2;
    }

    memset(block_buffer, 0, ELIXIR_BLOCK_SIZE);
    uint8_t* bitmap = (uint8_t*)block_buffer;
    for (uint64_t i = 0; i < sb.data_start_block; ++i) {
        set_bitmap_bit(bitmap, i);
    }
    if (!write_blocks(port, bitmap, sb.bitmap_start_block, 1)) {
        kfree(block_buffer);
        return -3;
    }

    memset(block_buffer, 0, ELIXIR_BLOCK_SIZE);
    elixir_file_table_t* ft = (elixir_file_table_t*)block_buffer;
    ft->magic = ELIXIR_FILE_TABLE_MAGIC;
    ft->num_entries = 0;
    // The initial file table is the root directory
    if (!write_blocks(port, ft, sb.file_table_start_block, 1)) {
        kfree(block_buffer);
        return -4;
    }

    kfree(block_buffer);
    return 0;
}

elixir_fs_t* elixir_mount(HBA_PORT* port) {
    elixir_fs_t* fs = kmalloc(sizeof(elixir_fs_t));
    if (!fs) return NULL;

    void* block_buffer = kmalloc(ELIXIR_BLOCK_SIZE);
    if (!block_buffer) {
        kfree(fs);
        return NULL;
    }

    if (!read_blocks(port, block_buffer, 0, 1)) {
        kfree(block_buffer);
        kfree(fs);
        return NULL;
    }

    memcpy(&fs->sb, block_buffer, sizeof(elixir_superblock_t));
    kfree(block_buffer);

    if (fs->sb.magic != ELIXIR_SUPER_BLOCK_MAGIC) {
        kfree(fs);
        return NULL;
    }

    fs->port = port;
    fs->free_block_bitmap = kmalloc(fs->sb.bitmap_size_blocks * ELIXIR_BLOCK_SIZE);
    if (!fs->free_block_bitmap) {
        kfree(fs);
        return NULL;
    }

    if (!read_blocks(port, fs->free_block_bitmap, fs->sb.bitmap_start_block, fs->sb.bitmap_size_blocks)) {
        kfree(fs->free_block_bitmap);
        kfree(fs);
        return NULL;
    }

    return fs;
}

void elixir_unmount(elixir_fs_t* fs) {
    if (!fs) return;
    if (fs->free_block_bitmap) kfree(fs->free_block_bitmap);
    kfree(fs);
}

int elixir_create_file(elixir_fs_t* fs, const char* path, elixir_file_type_t type, const void* data, size_t size) {
    char path_copy[ELIXIR_MAX_FILENAME * ELIXIR_MAX_FILES];
    strcpy(path_copy, path);

    char* parent_path = path_copy;
    char* file_name = NULL;

    // Find the last '/' to separate parent path and file name
    char* last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        file_name = last_slash + 1;
        if (strlen(parent_path) == 0) parent_path = "/"; // Root directory
    } else {
        // No slash, assume current directory (not supported yet, default to root)
        parent_path = "/";
        file_name = path_copy;
    }

    if (strlen(file_name) >= ELIXIR_MAX_FILENAME) return -1; // File name too long

    elixir_file_table_t* parent_ft = NULL;
    uint64_t parent_ft_block = 0;
    elixir_file_entry_t* parent_entry = NULL;

    if (strcmp(parent_path, "/") == 0) {
        // Root directory
        parent_ft = kmalloc(ELIXIR_BLOCK_SIZE);
        if (!parent_ft) return -2;
        if (!read_blocks(fs->port, parent_ft, fs->sb.file_table_start_block, 1)) {
            kfree(parent_ft);
            return -3;
        }
        parent_ft_block = fs->sb.file_table_start_block;
    } else {
        parent_entry = elixir_get_file_entry_from_path(fs, parent_path, &parent_ft, &parent_ft_block);
        if (!parent_entry || parent_entry->type != ELIXIR_FILE_TYPE_DIRECTORY) {
            if (parent_entry) kfree(parent_entry);
            if (parent_ft) kfree(parent_ft);
            return -4; // Parent not found or not a directory
        }
        kfree(parent_entry);
    }

    if (!parent_ft) return -5; // Should not happen

    // Check if file/directory already exists in parent
    for (uint32_t i = 0; i < parent_ft->num_entries; ++i) {
        if (strcmp(parent_ft->entries[i].name, file_name) == 0) {
            kfree(parent_ft);
            return -6; // File/directory exists
        }
    }

    if (parent_ft->num_entries >= ELIXIR_MAX_FILES) {
        kfree(parent_ft);
        return -7; // Parent directory full
    }

    int64_t start_block = find_free_block(fs);
    if (start_block < 0) {
        kfree(parent_ft);
        return -8; // No free blocks
    }

    uint64_t num_blocks = 0;
    if (type == ELIXIR_FILE_TYPE_FILE) {
        num_blocks = (size + ELIXIR_BLOCK_SIZE - 1) / ELIXIR_BLOCK_SIZE;
        if (num_blocks == 0) num_blocks = 1;

        void* block_buffer = kmalloc(num_blocks * ELIXIR_BLOCK_SIZE);
        if (!block_buffer) {
            kfree(parent_ft);
            return -9;
        }
        memset(block_buffer, 0, num_blocks * ELIXIR_BLOCK_SIZE);
        if (data && size > 0) {
            memcpy(block_buffer, data, size);
        }

        if (!write_blocks(fs->port, block_buffer, start_block, num_blocks)) {
            kfree(block_buffer);
            kfree(parent_ft);
            return -10;
        }
        kfree(block_buffer);
    } else if (type == ELIXIR_FILE_TYPE_DIRECTORY) {
        num_blocks = 1; // A directory is one block for its file table
        elixir_file_table_t* new_dir_ft = kmalloc(ELIXIR_BLOCK_SIZE);
        if (!new_dir_ft) {
            kfree(parent_ft);
            return -11;
        }
        memset(new_dir_ft, 0, ELIXIR_BLOCK_SIZE);
        new_dir_ft->magic = ELIXIR_FILE_TABLE_MAGIC;
        new_dir_ft->num_entries = 0;
        if (!write_blocks(fs->port, new_dir_ft, start_block, 1)) {
            kfree(new_dir_ft);
            kfree(parent_ft);
            return -12;
        }
        kfree(new_dir_ft);
    } else {
        kfree(parent_ft);
        return -13; // Invalid file type
    }

    elixir_file_entry_t* fe = &parent_ft->entries[parent_ft->num_entries];
    strcpy(fe->name, file_name);
    fe->type = type;
    fe->start_block = start_block;
    fe->size_bytes = (type == ELIXIR_FILE_TYPE_FILE) ? size : 0;
    fe->size_blocks = num_blocks;
    parent_ft->num_entries++;

    set_bitmap_bit(fs->free_block_bitmap, start_block);
    fs->sb.free_blocks -= num_blocks;

    if (!write_blocks(fs->port, parent_ft, parent_ft_block, 1)) {
        kfree(parent_ft);
        return -14;
    }
    kfree(parent_ft);

    if (!write_blocks(fs->port, fs->free_block_bitmap, fs->sb.bitmap_start_block, fs->sb.bitmap_size_blocks)) {
        return -15;
    }

    void* sb_buffer = kmalloc(ELIXIR_BLOCK_SIZE);
    if (!sb_buffer) return -16;
    memset(sb_buffer, 0, ELIXIR_BLOCK_SIZE);
    memcpy(sb_buffer, &fs->sb, sizeof(elixir_superblock_t));
    if (!write_blocks(fs->port, sb_buffer, 0, 1)) {
        kfree(sb_buffer);
        return -17;
    }
    kfree(sb_buffer);

    return 0;
}

void* elixir_read_file(elixir_fs_t* fs, const char* path, size_t* out_size) {
    elixir_file_entry_t* fe = elixir_get_file_entry_from_path(fs, path, NULL, NULL);
    if (!fe) return NULL; // File not found

    if (fe->type == ELIXIR_FILE_TYPE_DIRECTORY) {
        kfree(fe);
        return NULL; // Cannot read a directory as a file
    }

    void* buffer = kmalloc(fe->size_blocks * ELIXIR_BLOCK_SIZE);
    if (!buffer) {
        kfree(fe);
        return NULL;
    }

    if (!read_blocks(fs->port, buffer, fe->start_block, fe->size_blocks)) {
        kfree(buffer);
        kfree(fe);
        return NULL;
    }

    if (out_size) *out_size = fe->size_bytes;

    void* final_buffer = kmalloc(fe->size_bytes);
    if (!final_buffer) {
        kfree(buffer);
        kfree(fe);
        return NULL;
    }
    memcpy(final_buffer, buffer, fe->size_bytes);
    kfree(buffer);
    kfree(fe);

    return final_buffer;
}

int elixir_delete_file(elixir_fs_t* fs, const char* path) {
    elixir_file_table_t* parent_ft = NULL;
    uint64_t parent_ft_block = 0;
    elixir_file_entry_t* fe_to_delete = elixir_get_file_entry_from_path(fs, path, &parent_ft, &parent_ft_block);

    if (!fe_to_delete) return -1; // File or directory not found

    if (fe_to_delete->type == ELIXIR_FILE_TYPE_DIRECTORY) {
        // Read the directory's file table to check if it's empty
        elixir_file_table_t* dir_ft = kmalloc(ELIXIR_BLOCK_SIZE);
        if (!dir_ft) { kfree(fe_to_delete); if (parent_ft) kfree(parent_ft); return -2; }
        if (!read_blocks(fs->port, dir_ft, fe_to_delete->start_block, 1)) {
            kfree(dir_ft); kfree(fe_to_delete); if (parent_ft) kfree(parent_ft); return -3;
        }
        if (dir_ft->num_entries > 0) {
            kfree(dir_ft); kfree(fe_to_delete); if (parent_ft) kfree(parent_ft); return -4; // Directory not empty
        }
        kfree(dir_ft);
    }

    // Find the entry in the parent's file table
    int found_idx = -1;
    for (uint32_t i = 0; i < parent_ft->num_entries; ++i) {
        if (strcmp(parent_ft->entries[i].name, fe_to_delete->name) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) { // Should not happen if fe_to_delete was found
        kfree(fe_to_delete); if (parent_ft) kfree(parent_ft); return -5;
    }

    // Clear bitmap bits for the file's/directory's blocks
    for (uint64_t i = 0; i < fe_to_delete->size_blocks; ++i) {
        clear_bitmap_bit(fs->free_block_bitmap, fe_to_delete->start_block + i);
    }
    fs->sb.free_blocks += fe_to_delete->size_blocks;

    // Shift entries in parent to fill the gap
    for (uint32_t i = found_idx; i < parent_ft->num_entries - 1; ++i) {
        memcpy(&parent_ft->entries[i], &parent_ft->entries[i+1], sizeof(elixir_file_entry_t));
    }
    parent_ft->num_entries--;

    // Write updated parent file table, bitmap, and superblock
    if (!write_blocks(fs->port, parent_ft, parent_ft_block, 1)) {
        kfree(fe_to_delete); kfree(parent_ft); return -6;
    }
    kfree(parent_ft);

    if (!write_blocks(fs->port, fs->free_block_bitmap, fs->sb.bitmap_start_block, fs->sb.bitmap_size_blocks)) {
        kfree(fe_to_delete); return -7;
    }

    void* sb_buffer = kmalloc(ELIXIR_BLOCK_SIZE);
    if (!sb_buffer) { kfree(fe_to_delete); return -8; }
    memset(sb_buffer, 0, ELIXIR_BLOCK_SIZE);
    memcpy(sb_buffer, &fs->sb, sizeof(elixir_superblock_t));
    if (!write_blocks(fs->port, sb_buffer, 0, 1)) {
        kfree(sb_buffer); kfree(fe_to_delete); return -9;
    }
    kfree(sb_buffer);
    kfree(fe_to_delete);

    return 0;
}

int elixir_rename_file(elixir_fs_t* fs, const char* old_path, const char* new_path) {
    char old_path_copy[ELIXIR_MAX_FILENAME * ELIXIR_MAX_FILES];
    strcpy(old_path_copy, old_path);
    char* old_parent_path = old_path_copy;
    char* old_file_name = NULL;
    char* last_slash_old = strrchr(old_path_copy, '/');
    if (last_slash_old) {
        *last_slash_old = '\0';
        old_file_name = last_slash_old + 1;
        if (strlen(old_parent_path) == 0) old_parent_path = "/";
    } else {
        old_parent_path = "/";
        old_file_name = old_path_copy;
    }

    char new_path_copy[ELIXIR_MAX_FILENAME * ELIXIR_MAX_FILES];
    strcpy(new_path_copy, new_path);
    char* new_parent_path = new_path_copy;
    char* new_file_name = NULL;
    char* last_slash_new = strrchr(new_path_copy, '/');
    if (last_slash_new) {
        *last_slash_new = '\0';
        new_file_name = last_slash_new + 1;
        if (strlen(new_parent_path) == 0) new_parent_path = "/";
    } else {
        new_parent_path = "/";
        new_file_name = new_path_copy;
    }

    if (strlen(new_file_name) >= ELIXIR_MAX_FILENAME) return -1; // New file name too long

    // For now, only allow renaming within the same directory
    if (strcmp(old_parent_path, new_parent_path) != 0) {
        return -2; // Moving between directories not supported yet
    }

    elixir_file_table_t* parent_ft = NULL;
    uint64_t parent_ft_block = 0;
    elixir_file_entry_t* fe_to_rename = elixir_get_file_entry_from_path(fs, old_path, &parent_ft, &parent_ft_block);

    if (!fe_to_rename) { if (parent_ft) kfree(parent_ft); return -3; } // Old file not found

    // Check if new_name already exists in the same directory
    for (uint32_t i = 0; i < parent_ft->num_entries; ++i) {
        if (strcmp(parent_ft->entries[i].name, new_file_name) == 0) {
            kfree(fe_to_rename); kfree(parent_ft); return -4; // New name already exists
        }
    }

    // Find the entry in the parent's file table and update its name
    int found_idx = -1;
    for (uint32_t i = 0; i < parent_ft->num_entries; ++i) {
        if (strcmp(parent_ft->entries[i].name, old_file_name) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) { // Should not happen if fe_to_rename was found
        kfree(fe_to_rename); kfree(parent_ft); return -5;
    }

    strcpy(parent_ft->entries[found_idx].name, new_file_name);

    if (!write_blocks(fs->port, parent_ft, parent_ft_block, 1)) {
        kfree(fe_to_rename); kfree(parent_ft); return -6;
    }
        return 0;
    }
    
    int elixir_mkdir(elixir_fs_t* fs, const char* path) {
        return elixir_create_file(fs, path, ELIXIR_FILE_TYPE_DIRECTORY, NULL, 0);
    }
    
    int elixir_rmdir(elixir_fs_t* fs, const char* path) {
            return elixir_delete_file(fs, path);
        }
        
        elixir_file_entry_t* elixir_list_directory(elixir_fs_t* fs, const char* path, uint32_t* num_entries) {
            elixir_file_table_t* target_ft = NULL;
            uint64_t target_ft_block = 0;
            elixir_file_entry_t* target_entry = elixir_get_file_entry_from_path(fs, path, &target_ft, &target_ft_block);
        
            if (!target_entry) { // Path not found
                if (target_ft) kfree(target_ft);
                if (num_entries) *num_entries = 0;
                return NULL;
            }
        
            if (target_entry->type == ELIXIR_FILE_TYPE_FILE) { // Not a directory
                kfree(target_entry);
                if (target_ft) kfree(target_ft);
                if (num_entries) *num_entries = 0;
                return NULL;
            }
        
            // At this point, target_entry is a directory, and target_ft holds its file table
            if (!target_ft) { // Should not happen if target_entry is a directory
                kfree(target_entry);
                if (num_entries) *num_entries = 0;
                return NULL;
            }
        
            elixir_file_entry_t* entries_copy = kmalloc(target_ft->num_entries * sizeof(elixir_file_entry_t));
            if (!entries_copy) {
                kfree(target_entry);
                kfree(target_ft);
                if (num_entries) *num_entries = 0;
                return NULL;
            }
        
            memcpy(entries_copy, target_ft->entries, target_ft->num_entries * sizeof(elixir_file_entry_t));
            if (num_entries) *num_entries = target_ft->num_entries;
        
            kfree(target_entry);
            kfree(target_ft);
        
            return entries_copy;
        }
            

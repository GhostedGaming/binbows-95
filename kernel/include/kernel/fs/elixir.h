#ifndef ELIXIR_H
#define ELIXIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sata.h>

#define ELIXIR_SUPER_BLOCK_MAGIC 0x454C4658
#define ELIXIR_FILE_TABLE_MAGIC 0x454C4958
#define ELIXIR_MAX_FILENAME 255
#define ELIXIR_MAX_FILES 1024
#define ELIXIR_BLOCK_SIZE 4096

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t total_blocks;
    uint64_t free_blocks;

    uint64_t bitmap_start_block;
    uint64_t bitmap_size_blocks;

    uint64_t file_table_start_block;
    uint64_t file_table_size_blocks;

    uint64_t data_start_block;
} __attribute__((packed)) elixir_superblock_t;

typedef enum {
    ELIXIR_FILE_TYPE_FILE,
    ELIXIR_FILE_TYPE_DIRECTORY
} elixir_file_type_t;

typedef struct {
    char name[ELIXIR_MAX_FILENAME];
    elixir_file_type_t type;
    uint64_t start_block;
    uint64_t size_bytes;
    uint64_t size_blocks;
} __attribute__((packed)) elixir_file_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t num_entries;
    elixir_file_entry_t entries[ELIXIR_MAX_FILES];
} __attribute__((packed)) elixir_file_table_t;

typedef struct {
    HBA_PORT* port;
    elixir_superblock_t sb;
    uint8_t* free_block_bitmap;
} elixir_fs_t;

int elixir_format(HBA_PORT* port, uint64_t device_size_bytes);
elixir_fs_t* elixir_mount(HBA_PORT* port);
void elixir_unmount(elixir_fs_t* fs);

int elixir_create_file(elixir_fs_t* fs, const char* path, elixir_file_type_t type, const void* data, size_t size);
void* elixir_read_file(elixir_fs_t* fs, const char* path, size_t* out_size);
int elixir_delete_file(elixir_fs_t* fs, const char* path);
int elixir_rename_file(elixir_fs_t* fs, const char* old_path, const char* new_path);
int elixir_mkdir(elixir_fs_t* fs, const char* path);
int elixir_rmdir(elixir_fs_t* fs, const char* path);
elixir_file_entry_t* elixir_list_directory(elixir_fs_t* fs, const char* path, uint32_t* num_entries);

#endif // ELIXIR_H

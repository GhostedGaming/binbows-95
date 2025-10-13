#ifndef ELIXIR_H
#define ELIXIR_H

#include <stdint.h>
#include <stddef.h>
#include <sata.h>

#define SUPER_BLOCK_MAGIC 0x454C4658
#define FILE_TABLE_MAGIC 0x454C4958

#define SUCCESS 0
#define FAILED_TO_CREATE_SUPER_BLOCK -1
#define FAILED_TO_WRITE_SUPER_BLOCK -2
#define FAILED_TO_READ_SUPER_BLOCK -3
#define FAILED_TO_ALLOCATE_FILE_TABLE -4
#define FAILED_TO_WRITE_FILE_TABLE -5
#define FAILED_TO_READ_FILE_TABLE -6
#define FAILED_FILE_TABLE_VERIFICATION -7
#define FAILED_TO_WRITE_FILE -8

#define MAX_FILENAME 255
#define MAX_FILES 1024

struct super_block {
    uint32_t magic;
    uint32_t version;
    uint64_t total_blocks;
    uint32_t block_size;
    uint64_t file_table_start_block;
    uint64_t file_table_size_blocks;
    uint64_t data_start_block;
    uint64_t free_blocks;
    uint8_t reserved[472];
} __attribute__((packed));

struct file_entry {
    char name[MAX_FILENAME];
    uint32_t start_block;
    uint32_t size_blocks;
    uint64_t size_bytes;
} __attribute__((packed));

struct file_table {
    uint32_t magic;
    uint32_t max_entries;
    uint32_t num_entries;
    uint32_t reserved;
    struct file_entry entries[MAX_FILES];
} __attribute__((packed));

struct elixir_init_data {
    struct super_block* sb;
    struct file_table* ft;
    uint8_t sectors_per_block;
    uint64_t file_table_start_sector;
    uint64_t file_table_sectors;
};

struct super_block* create_super_block(uint64_t total_blocks, uint32_t block_size);
void destroy_super_block(struct super_block* sb);
struct file_table* create_file_table();
void destroy_file_table(struct file_table* ft);
void destroy_file(struct file_entry* fe);
int new_entry(struct file_table* ft, struct file_entry* fe);
struct file_table* load_file_table(HBA_PORT *port);
int save_file_table(HBA_PORT *port, struct file_table* ft);
int create_file(char* name, HBA_PORT *port, void* data, size_t data_size);
void* read_file(char* name, HBA_PORT *port, size_t* out_size);
struct elixir_init_data* init_elixir(HBA_PORT *port);
int write_elixir_to_disk(HBA_PORT *port, struct elixir_init_data* init_data);
void destroy_elixir_init_data(struct elixir_init_data* init_data);

#endif
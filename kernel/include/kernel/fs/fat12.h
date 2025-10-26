#ifndef FAT12_H
#define FAT12_H

#include <stdint.h>
#include <stdbool.h>
#include <ide.h>
#include <mem.h>
#include <util.h>

#define MAX_FILE_NAME 11
#define SECTOR_SIZE 512
#define FAT12_MAX_CLUSTERS 4085

typedef struct {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  file_system_type[8];
} __attribute__((packed)) bpb_t12;

typedef struct {
    char name[MAX_FILE_NAME];
    uint8_t attr;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat12_dir_entry_t;

// File attribute flags
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LFN (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// FAT12 end-of-cluster marker
#define FAT12_EOC 0xFF8

extern bpb_t12 cached_bpb12;
extern bool bpb_cached12;
extern uint8_t cached_drive12;

uint8_t* fat12_read_file(uint8_t drive, const char *filename, uint32_t *size_out);
bool fat12_read_file_to_buffer(uint8_t drive, const char *filename, uint8_t *buffer, uint32_t *size_out);
int fat12_write_file(uint8_t drive, const char *filename, const uint8_t *data, uint32_t size);
char* fat12_read_files(uint8_t drive);

void format_fat12(uint8_t drive, ...);
void fat_init(uint8_t drive, uint8_t reserved_sector_count, uint8_t num_fats, uint8_t fat_size_16);

uint16_t fat12_alloc_clusters(uint8_t drive, uint16_t count);
void fat12_write_clusters(uint8_t drive, uint16_t first_cluster, const uint8_t *data, uint32_t size);

void fat12_write_dir_entry(uint8_t drive, bpb_t12* bpb, int index, const char* filename, uint16_t first_cluster, uint32_t size);
void fat12_write_subdir(uint8_t drive, bpb_t12* bpb, int parent_dir_index, const char* dirname, uint16_t first_cluster);
int fat12_find_free_root_dir_entry(uint8_t drive, bpb_t12* bpb);

int read_bpb(uint8_t drive, bpb_t12 *bpb);
bool validate_bpb(bpb_t12 *bpb);
uint16_t fat12_read_entry(const uint8_t* fat, uint16_t cluster);
void fat12_write_entry(uint8_t* fat, uint16_t cluster, uint16_t value);
int fat12_find_free_cluster(uint8_t* fat, uint16_t max_clusters);
void fat12_write_fat(uint8_t drive, uint8_t num_fats, uint16_t fat_size_16, const uint8_t* fat_data, uint8_t reserved_sector_count);
void fat12_format_filename(const char* filename, char* fat_name);
void format_filename_for_compare(const char* filename, char* fat_name);

#endif // FAT12_H
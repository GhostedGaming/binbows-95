#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>
#include <ide.h>
#include <mem.h>
#include <util.h>

#define MAX_FILE_NAME 11
#define SECTOR_SIZE 512
#define FAT32_MIN_CLUSTERS 65525
#define FAT32_MAX_CLUSTERS 0x0FFFFFF6

// BIOS Parameter Block for FAT32
typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count; // always 0 for FAT32
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16; // zero for FAT32
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // FAT32 Extended BPB
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t file_system_type[8];
} __attribute__((packed)) bpb_t;

// Directory Entry structure
typedef struct {
    char name[MAX_FILE_NAME];       // 8.3 filename format (no null terminator)
    uint8_t attr;
    uint8_t nt_reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

// File attribute flags
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_LFN       (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// FAT32 end-of-cluster markers
#define FAT32_EOC        0x0FFFFFF8
#define FAT32_EOC_MAX    0x0FFFFFFF
#define FAT32_BAD_CLUSTER 0x0FFFFFF7
#define FAT32_FREE_CLUSTER 0x00000000

// Global BPB cache
static bpb_t cached_bpb;
static bool bpb_cached = false;
static uint8_t cached_drive = 0xFF;

// Function declarations
void format_fat32(uint8_t drive);
uint32_t fat32_alloc_clusters(uint8_t drive, uint32_t count);
void fat32_write_clusters(uint8_t drive, uint32_t first_cluster, const uint8_t *data, uint32_t size);
void fat32_write_dir_entry(uint8_t drive, bpb_t* bpb, uint32_t parent_cluster, const char* filename, uint32_t first_cluster, uint32_t size);
int fat32_write_file(uint8_t drive, const char *filename, const uint8_t *data, uint32_t size);
bool fat32_read_file(uint8_t drive, const char *filename, uint8_t *buffer, uint32_t *size_out);

// Internal helper functions
int read_bpb32(uint8_t drive, bpb_t *bpb);
uint32_t fat32_read_entry(const uint8_t* fat, uint32_t cluster);
uint32_t cluster_to_lba(bpb_t* bpb, uint32_t cluster);
void fat32_write_entry(uint8_t* fat, uint32_t cluster, uint32_t value);
int fat32_find_free_cluster(uint8_t* fat, uint32_t max_clusters);
void fat32_write_fat(uint8_t drive, uint8_t num_fats, uint32_t fat_size_32, const uint8_t* fat_data, uint16_t reserved_sector_count);
int fat32_find_free_dir_entry(uint8_t drive, bpb_t* bpb, uint32_t dir_cluster);
void fat32_format_filename(const char* filename, char* fat_name);
void fat32_format_filename_for_compare(const char* filename, char* fat_name);
bool validate_bpb32(bpb_t *bpb);
void fat32_init(uint8_t drive, uint32_t reserved_sector_count, uint8_t num_fats, uint32_t fat_size_32);

#endif // FAT32_H

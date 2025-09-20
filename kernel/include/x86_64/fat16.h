#ifndef FAT16_H
#define FAT16_H
#include <stdint.h>
#include <stdbool.h>
#include <ide.h>
#include <mem.h>
#include <util.h>

#define MAX_FILE_NAME 11
#define SECTOR_SIZE 512
#define FAT16_MIN_CLUSTERS 4085
#define FAT16_MAX_CLUSTERS 65525

// BIOS Parameter Block for FAT16
typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    // Extended Boot Record
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t file_system_type[8];
} __attribute__((packed)) bpb_t16;

// Directory Entry structure
typedef struct {
    char name[MAX_FILE_NAME];       // 8.3 filename format (no null terminator)
    uint8_t attr;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;    // Always 0 in FAT16
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat16_dir_entry_t;

// File attribute flags
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LFN (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// FAT16 end-of-cluster markers
#define FAT16_EOC 0xFFF8
#define FAT16_EOC_MAX 0xFFFF
#define FAT16_BAD_CLUSTER 0xFFF7
#define FAT16_FREE_CLUSTER 0x0000

// Global BPB cache to avoid repeated reads
static bpb_t16 cached_bpb16;
static bool bpb_cached16 = false;
static uint8_t cached_drive16 = 0xFF;

// Function declarations
void format_fat16(uint8_t drive);
uint16_t fat16_alloc_clusters(uint8_t drive, uint16_t count);
void fat16_write_clusters(uint8_t drive, uint16_t first_cluster, const uint8_t *data, uint32_t size);
void fat16_write_dir_entry(uint8_t drive, bpb_t16* bpb, int index, const char* filename, uint16_t first_cluster, uint32_t size);
int fat16_write_file(uint8_t drive, const char *filename, const uint8_t *data, uint32_t size);
bool fat16_read_file(uint8_t drive, const char *filename, uint8_t *buffer, uint32_t *size_out);

// Internal helper functions
int read_bpb16(uint8_t drive, bpb_t16 *bpb);
uint16_t fat16_read_entry(const uint8_t* fat, uint16_t cluster);
void fat16_write_entry(uint8_t* fat, uint16_t cluster, uint16_t value);
int fat16_find_free_cluster(uint8_t* fat, uint16_t max_clusters);
void fat16_write_fat(uint8_t drive, uint8_t num_fats, uint16_t fat_size_16, const uint8_t* fat_data, uint8_t reserved_sector_count);
int fat16_find_free_root_dir_entry(uint8_t drive, bpb_t16* bpb);
void fat16_format_filename(const char* filename, char* fat_name);
void fat16_format_filename_for_compare(const char* filename, char* fat_name);
bool validate_bpb16(bpb_t16 *bpb);
void fat16_init(uint8_t drive, uint8_t reserved_sector_count, uint8_t num_fats, uint16_t fat_size_16);

#endif // FAT16_H
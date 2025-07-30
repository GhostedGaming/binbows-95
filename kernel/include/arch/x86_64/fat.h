#ifndef FAT_H
#define FAT_H

#include <stdint.h>

// FAT32 Boot Sector structure
struct __attribute__((packed)) fat32_boot_sector {
    uint8_t jump[3];               // Jump instruction
    uint8_t oem_name[8];           // OEM name
    uint16_t bytes_per_sector;     // Bytes per sector
    uint8_t sectors_per_cluster;   // Sectors per cluster
    uint16_t reserved_sectors;     // Reserved sectors
    uint8_t num_fats;              // Number of FATs
    uint16_t root_entries;         // Root directory entries (0 for FAT32)
    uint16_t total_sectors_16;     // Total sectors (0 for FAT32)
    uint8_t media_descriptor;      // Media descriptor
    uint16_t sectors_per_fat_16;   // Sectors per FAT (0 for FAT32)
    uint16_t sectors_per_track;    // Sectors per track
    uint16_t num_heads;            // Number of heads
    uint32_t hidden_sectors;       // Hidden sectors
    uint32_t total_sectors_32;     // Total sectors
    uint32_t sectors_per_fat_32;   // Sectors per FAT
    uint16_t flags;                // Flags
    uint16_t version;              // Version
    uint32_t root_cluster;         // Root directory cluster
    uint16_t fsinfo_sector;        // FSInfo sector
    uint16_t backup_boot_sector;   // Backup boot sector
    uint8_t reserved[12];          // Reserved
    uint8_t drive_number;          // Drive number
    uint8_t reserved1;             // Reserved
    uint8_t boot_signature;        // Boot signature
    uint32_t volume_serial;        // Volume serial number
    uint8_t volume_label[11];      // Volume label
    uint8_t fs_type[8];            // File system type
    uint8_t boot_code[420];        // Boot code
    uint16_t boot_sector_signature; // Boot sector signature (0xAA55)
};

// FAT32 FSInfo structure
struct __attribute__((packed)) fat32_fsinfo {
    uint32_t lead_signature;       // Lead signature (0x41615252)
    uint8_t reserved1[480];        // Reserved
    uint32_t struct_signature;     // Structure signature (0x61417272)
    uint32_t free_count;           // Free cluster count
    uint32_t next_free;            // Next free cluster
    uint8_t reserved2[12];         // Reserved
    uint32_t trail_signature;      // Trail signature (0xAA550000)
};

// FAT32 Directory Entry structure
struct __attribute__((packed)) fat32_dir_entry {
    uint8_t name[11];              // File name (8.3 format)
    uint8_t attributes;            // File attributes
    uint8_t reserved;              // Reserved
    uint8_t creation_time_tenth;   // Creation time (tenths of second)
    uint16_t creation_time;        // Creation time
    uint16_t creation_date;        // Creation date
    uint16_t last_access_date;     // Last access date
    uint16_t cluster_high;         // High 16 bits of cluster number
    uint16_t last_write_time;      // Last write time
    uint16_t last_write_date;      // Last write date
    uint16_t cluster_low;          // Low 16 bits of cluster number
    uint32_t file_size;            // File size
};

// Long File Name (LFN) entry structure
struct __attribute__((packed)) fat32_lfn_entry {
    uint8_t order;                 // Order of this entry in sequence
    uint16_t name1[5];             // First 5 characters (UTF-16)
    uint8_t attributes;            // Attributes (always 0x0F for LFN)
    uint8_t type;                  // Type (always 0 for LFN)
    uint8_t checksum;              // Checksum of 8.3 name
    uint16_t name2[6];             // Next 6 characters (UTF-16)
    uint16_t first_cluster;        // First cluster (always 0 for LFN)
    uint16_t name3[2];             // Last 2 characters (UTF-16)
};

// FAT32 File System Information structure
struct fat32_fs {
    struct fat32_boot_sector *boot_sector;
    struct fat32_fsinfo *fsinfo;
    uint32_t *fat_table;           // Cached FAT table
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t total_clusters;
    uint32_t free_clusters;        // Cached free cluster count
    uint32_t next_free_cluster;    // Next free cluster hint
};

// File handle structure
struct fat32_file {
    struct fat32_fs *fs;
    uint32_t first_cluster;
    uint32_t current_cluster;
    uint32_t file_size;
    uint32_t position;
    uint8_t attributes;
    char name[256];                // Long filename support
};

// File attribute flags
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LONG_NAME  0x0F

// FAT32 cluster markers
#define FAT32_END_OF_CHAIN  0x0FFFFFFF
#define FAT32_BAD_CLUSTER   0x0FFFFFF7
#define FAT32_FREE_CLUSTER  0x00000000

// FAT32 signatures
#define FAT32_BOOT_SIGNATURE        0xAA55
#define FAT32_FSINFO_LEAD_SIG       0x41615252
#define FAT32_FSINFO_STRUCT_SIG     0x61417272
#define FAT32_FSINFO_TRAIL_SIG      0xAA550000

// LFN constants
#define LFN_LAST_ENTRY      0x40
#define LFN_DELETED         0xE5

// Error codes
#define FAT32_SUCCESS       0
#define FAT32_ERROR         -1
#define FAT32_NOT_FOUND     -2
#define FAT32_NO_SPACE      -3
#define FAT32_INVALID       -4

// Function declarations
int fat32_init(struct fat32_fs *fs, void *boot_sector);
uint32_t fat32_get_next_cluster(struct fat32_fs *fs, uint32_t cluster);
int fat32_read_cluster(struct fat32_fs *fs, uint32_t cluster, void *buffer);
int fat32_write_cluster(struct fat32_fs *fs, uint32_t cluster, void *buffer);
struct fat32_dir_entry *fat32_find_file(struct fat32_fs *fs, const char *filename);
int fat32_open_file(struct fat32_fs *fs, const char *filename, struct fat32_file *file);
int fat32_read_file(struct fat32_file *file, void *buffer, uint32_t size);
int fat32_write_file(struct fat32_file *file, void *buffer, uint32_t size);
int fat32_close_file(struct fat32_file *file);
uint32_t fat32_allocate_cluster(struct fat32_fs *fs);
int fat32_free_cluster_chain(struct fat32_fs *fs, uint32_t cluster);
int fat32_create_file(struct fat32_fs *fs, const char *filename);
int fat32_delete_file(struct fat32_fs *fs, const char *filename);

#endif // FAT_H
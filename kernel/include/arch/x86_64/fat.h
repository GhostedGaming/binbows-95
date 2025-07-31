#ifndef FAT_H
#define FAT_H

#include <stdint.h>

#define SECTOR_SIZE 512

typedef struct __attribute__((packed)) {
    uint8_t  jmpBoot[3];
    uint8_t  OEMName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numFATs;
    uint16_t rootEntryCount;
    uint16_t totalSectors16;
    uint8_t  media;
    uint16_t FATSize16;
    uint16_t sectorsPerTrack;
    uint16_t numHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;
    uint8_t  driveNumber;
    uint8_t  reserved1;
    uint8_t  bootSignature;
    uint32_t volumeID;
    uint8_t  volumeLabel[11];
    uint8_t  fileSystemType[8];
} FAT16_BPB;

typedef struct __attribute__((packed)) {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  creationTimeTenth;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t lastAccessDate;
    uint16_t firstClusterHigh;
    uint16_t writeTime;
    uint16_t writeDate;
    uint16_t firstClusterLow;
    uint32_t fileSize;
} FAT16_DirectoryEntry;

#define FAT16_FREE_CLUSTER     0x0000
#define FAT16_BAD_CLUSTER      0xFFF7
#define FAT16_EOC_MARKER       0xFFF8

#define ATTR_READ_ONLY   0x01
#define ATTR_HIDDEN      0x02
#define ATTR_SYSTEM      0x04
#define ATTR_VOLUME_ID   0x08
#define ATTR_DIRECTORY   0x10
#define ATTR_ARCHIVE     0x20
#define ATTR_LONG_NAME   (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

int fat16_init(void);
int fat16_format(uint32_t total_sectors);

int fat16_find_file(const char* name, FAT16_DirectoryEntry* outEntry);
int fat16_read_file(FAT16_DirectoryEntry* fileEntry, uint8_t* buffer, uint32_t bufferSize);

#endif // FAT_H
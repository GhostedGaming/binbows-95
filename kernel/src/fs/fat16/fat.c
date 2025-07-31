#include <fat.h>
#include <memory.h>

// These must be implemented elsewhere
extern int ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint8_t* buffer);
extern int ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const uint8_t* buffer);

static FAT16_BPB bpb;
static uint32_t firstDataSector;
static uint32_t fatStartSector;
static uint32_t rootDirStartSector;
static uint32_t rootDirSectors;
static uint32_t totalSectors;
static uint32_t sectorsPerCluster;

static uint8_t sector_buffer[SECTOR_SIZE];

static int read_sector(uint32_t lba) {
    return ide_read_sectors(0, lba, 1, sector_buffer);
}

static int write_sector(uint32_t lba, const uint8_t* data) {
    return ide_write_sectors(0, lba, 1, data);
}

int fat16_init() {
    if (read_sector(0) != 0) {
        return -1;
    }

    FAT16_BPB* p = (FAT16_BPB*)sector_buffer;

    if (p->bytesPerSector != SECTOR_SIZE) {
        return -2;
    }

    memcpy(&bpb, p, sizeof(FAT16_BPB));

    sectorsPerCluster = bpb.sectorsPerCluster;

    rootDirSectors = ((bpb.rootEntryCount * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;

    fatStartSector = bpb.reservedSectorCount;

    rootDirStartSector = fatStartSector + (bpb.numFATs * bpb.FATSize16);

    firstDataSector = rootDirStartSector + rootDirSectors;

    if (bpb.totalSectors16 != 0) {
        totalSectors = bpb.totalSectors16;
    } else {
        totalSectors = bpb.totalSectors32;
    }

    return 0;
}

static uint32_t cluster_to_sector(uint16_t cluster) {
    return firstDataSector + (cluster - 2) * sectorsPerCluster;
}

static uint16_t fat16_read_fat_entry(uint16_t cluster) {
    uint32_t fatOffset = cluster * 2;
    uint32_t fatSector = fatStartSector + (fatOffset / SECTOR_SIZE);
    uint32_t entOffset = fatOffset % SECTOR_SIZE;

    if (read_sector(fatSector) != 0) {
        return FAT16_BAD_CLUSTER;
    }

    uint16_t entry = *(uint16_t*)&sector_buffer[entOffset];
    return entry;
}

int fat16_read_root_dir_entry(uint32_t index, FAT16_DirectoryEntry* dirEntry) {
    uint32_t entriesPerSector = SECTOR_SIZE / sizeof(FAT16_DirectoryEntry);
    uint32_t sectorNum = rootDirStartSector + (index / entriesPerSector);
    uint32_t offset = (index % entriesPerSector) * sizeof(FAT16_DirectoryEntry);

    if (read_sector(sectorNum) != 0) {
        return -1;
    }

    memcpy(dirEntry, &sector_buffer[offset], sizeof(FAT16_DirectoryEntry));
    return 0;
}

int fat16_find_file(const char* name, FAT16_DirectoryEntry* outEntry) {
    FAT16_DirectoryEntry entry;
    uint32_t maxEntries = bpb.rootEntryCount;

    for (uint32_t i = 0; i < maxEntries; i++) {
        if (fat16_read_root_dir_entry(i, &entry) != 0)
            return -1;

        if (entry.name[0] == 0x00) {
            break;
        }

        if ((uint8_t)entry.name[0] == 0xE5 ||
            (entry.attr & (ATTR_VOLUME_ID | ATTR_LONG_NAME)) != 0) {
            continue;
        }

        if (memcmp(entry.name, name, 11) == 0) {
            memcpy(outEntry, &entry, sizeof(FAT16_DirectoryEntry));
            return 0;
        }
    }
    return -1;
}

int fat16_read_file(FAT16_DirectoryEntry* fileEntry, uint8_t* buffer, uint32_t bufferSize) {
    uint16_t cluster = fileEntry->firstClusterLow;
    uint32_t bytesRead = 0;
    uint32_t fileSize = fileEntry->fileSize;

    while (cluster >= 2 && cluster < FAT16_EOC_MARKER) {
        uint32_t sector = cluster_to_sector(cluster);
        for (uint8_t i = 0; i < sectorsPerCluster; i++) {
            if (read_sector(sector + i) != 0) {
                return -1;
            }

            uint32_t bytesToCopy = SECTOR_SIZE;
            if (bytesRead + bytesToCopy > fileSize) {
                bytesToCopy = fileSize - bytesRead;
            }

            if (bytesRead + bytesToCopy > bufferSize) {
                bytesToCopy = bufferSize - bytesRead;
            }

            memcpy(buffer + bytesRead, sector_buffer, bytesToCopy);
            bytesRead += bytesToCopy;

            if (bytesRead >= fileSize || bytesRead >= bufferSize) {
                return bytesRead;
            }
        }

        cluster = fat16_read_fat_entry(cluster);
    }

    return bytesRead;
}

static FAT16_BPB fat16_create_bpb(uint32_t total_sectors) {
    FAT16_BPB bpb = {0};

    bpb.jmpBoot[0] = 0xEB;
    bpb.jmpBoot[1] = 0x3C;
    bpb.jmpBoot[2] = 0x90;

    memcpy(bpb.OEMName, "MSDOS5.0", 8);

    bpb.bytesPerSector = SECTOR_SIZE;
    bpb.sectorsPerCluster = 4;
    bpb.reservedSectorCount = 1;
    bpb.numFATs = 2;
    bpb.rootEntryCount = 512;
    bpb.media = 0xF8;

    bpb.totalSectors16 = (total_sectors <= 0xFFFF) ? total_sectors : 0;
    bpb.totalSectors32 = (total_sectors > 0xFFFF) ? total_sectors : 0;

    uint32_t rootDirSectors = ((bpb.rootEntryCount * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;
    uint32_t dataSectors = total_sectors - (bpb.reservedSectorCount + (bpb.numFATs * 0) + rootDirSectors);
    uint32_t countOfClusters = dataSectors / bpb.sectorsPerCluster;

    uint32_t fatSize = ((countOfClusters * 2) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;
    bpb.FATSize16 = fatSize;

    bpb.sectorsPerTrack = 63;
    bpb.numHeads = 255;
    bpb.hiddenSectors = 0;

    bpb.driveNumber = 0x80;
    bpb.bootSignature = 0x29;
    bpb.volumeID = 0x12345678;
    memcpy(bpb.volumeLabel, "NO NAME    ", 11);
    memcpy(bpb.fileSystemType, "FAT16   ", 8);

    return bpb;
}

int fat16_format(uint32_t total_sectors) {
    FAT16_BPB new_bpb = fat16_create_bpb(total_sectors);
    uint8_t sector[SECTOR_SIZE];
    memset(sector, 0, SECTOR_SIZE);

    // Write boot sector
    memcpy(sector, &new_bpb, sizeof(FAT16_BPB));
    sector[510] = 0x55;
    sector[511] = 0xAA;

    if (write_sector(0, sector) != 0) {
        return -1;
    }

    uint32_t rootDirSectors = ((new_bpb.rootEntryCount * 32) + (new_bpb.bytesPerSector - 1)) / new_bpb.bytesPerSector;
    uint32_t fatStart = new_bpb.reservedSectorCount;
    uint32_t rootDirStart = fatStart + (new_bpb.numFATs * new_bpb.FATSize16);

    // Initialize FAT tables
    memset(sector, 0, SECTOR_SIZE);

    // First 3 bytes of FAT: media descriptor and reserved clusters
    sector[0] = new_bpb.media;
    sector[1] = 0xFF;
    sector[2] = 0xFF;
    sector[3] = 0xFF;

    for (uint32_t fat = 0; fat < new_bpb.numFATs; fat++) {
        if (write_sector(fatStart + fat * new_bpb.FATSize16, sector) != 0) return -2;

        memset(sector, 0, SECTOR_SIZE);
        for (uint32_t i = 1; i < new_bpb.FATSize16; i++) {
            if (write_sector(fatStart + fat * new_bpb.FATSize16 + i, sector) != 0) return -3;
        }
    }

    // Clear root directory area
    memset(sector, 0, SECTOR_SIZE);
    for (uint32_t i = 0; i < rootDirSectors; i++) {
        if (write_sector(rootDirStart + i, sector) != 0) return -4;
    }

    // Save global bpb and info for later use
    memcpy(&bpb, &new_bpb, sizeof(FAT16_BPB));
    sectorsPerCluster = bpb.sectorsPerCluster;
    rootDirSectors = rootDirSectors;
    fatStartSector = fatStart;
    rootDirStartSector = rootDirStart;
    totalSectors = total_sectors;
    firstDataSector = rootDirStartSector + rootDirSectors;

    return 0;
}
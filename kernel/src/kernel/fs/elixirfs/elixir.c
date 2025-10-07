#include <elixir.h>
#include <serial.h>

#include <ide.h>

#define FAILED_TO_WRITE_SUPER_BLOCK 1;

int init_elixir(uint8_t drive_num) {
    uint64_t total_sectors = read_total_sectors(drive_num);
    uint32_t block_size = 4096;
    uint32_t sector_size = 512;
    
    uint64_t total_blocks = (total_sectors * sector_size) / block_size;
    
    struct super_block sb = create_super_block(total_blocks, block_size);

    uint8_t sectors_per_block = block_size / sector_size;
    if (ide_write_sectors(drive_num, sectors_per_block, 0, &sb) != 0) {
        serial_printf("Failed to write super block!\n");
        return FAILED_TO_WRITE_SUPER_BLOCK;
    }
}
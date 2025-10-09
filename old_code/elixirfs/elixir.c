#include <elixir.h>
#include <serial.h>
#include <ide.h>
#include <util.h>
#include <mem.h>

int init_elixir(uint8_t drive_num) {
    uint64_t total_sectors = read_total_sectors(drive_num);
    uint32_t block_size = 4096;
    uint32_t sector_size = 512;
    uint8_t sectors_per_block = block_size / sector_size;
    
    uint64_t total_blocks = (total_sectors * sector_size) / block_size;
    
    serial_printf("Initializing Elixir filesystem...\n");
    serial_printf("  Total sectors: %llu\n", total_sectors);
    serial_printf("  Total blocks: %llu\n", total_blocks);
    serial_printf("  Sectors per block: %u\n", sectors_per_block);

    struct super_block* sb = create_super_block(total_blocks, block_size);
    if (!sb) {
        serial_printf("Failed to create superblock!\n");
        return FAILED_TO_CREATE_SUPER_BLOCK;
    }
    
    serial_printf("Superblock values:\n");
    serial_printf("  Magic: 0x%x\n", sb->magic);
    serial_printf("  Total blocks: %llu\n", sb->total_blocks);
    serial_printf("  File table start block: %llu\n", sb->file_table_start_block);
    serial_printf("  File table size blocks: %llu\n", sb->file_table_size_blocks);
    serial_printf("  Data start block: %llu\n", sb->data_start_block);
    serial_printf("  Free blocks: %llu\n", sb->free_blocks);
    
    if (sb->magic != SUPER_BLOCK_MAGIC) {
        serial_printf("Failed to create valid superblock!\n");
        destroy_super_block(sb);
        return FAILED_TO_CREATE_SUPER_BLOCK;
    }

    serial_printf("\nWriting superblock to sector 0...\n");
    if (ide_write_sectors(drive_num, sectors_per_block, 0, sb) != 0) {
        serial_printf("Failed to write superblock!\n");
        destroy_super_block(sb);
        return FAILED_TO_WRITE_SUPER_BLOCK;
    }

    serial_printf("Superblock written successfully\n");
    
    struct super_block* verify_sb = kmalloc(sizeof(struct super_block));
    if (!verify_sb) {
        destroy_super_block(sb);
        return FAILED_TO_READ_SUPER_BLOCK;
    }
    
    serial_printf("\nReading superblock verification:\n");
    if (ide_read_sectors(drive_num, sectors_per_block, 0, verify_sb) != 0) {
        serial_printf("Failed to read superblock for verification!\n");
        kfree(verify_sb);
        destroy_super_block(sb);
        return FAILED_TO_READ_SUPER_BLOCK;
    }
    
    serial_printf("  Magic: 0x%x (expected 0x%x)\n", verify_sb->magic, SUPER_BLOCK_MAGIC);
    serial_printf("  Version: %u\n", verify_sb->version);
    serial_printf("  Total blocks: %llu\n", verify_sb->total_blocks);
    serial_printf("  Block size: %u\n", verify_sb->block_size);
    serial_printf("  File table start: %llu\n", verify_sb->file_table_start_block);
    serial_printf("  File table size: %llu\n", verify_sb->file_table_size_blocks);
    serial_printf("  Data start: %llu\n", verify_sb->data_start_block);
    serial_printf("  Free blocks: %llu\n", verify_sb->free_blocks);
    
    kfree(verify_sb);

    serial_printf("\nCreating file table...\n");
    struct file_table* ft = create_file_table();
    if (!ft) {
        serial_printf("Failed to allocate file table!\n");
        destroy_super_block(sb);
        return FAILED_TO_ALLOCATE_FILE_TABLE;
    }
    
    serial_printf("File table created:\n");
    serial_printf("  Size: %llu bytes\n", (uint64_t)sizeof(struct file_table));
    serial_printf("  Magic: 0x%x\n", ft->magic);
    serial_printf("  Max entries: %u\n", ft->max_entries);
    serial_printf("  Num entries: %u\n", ft->num_entries);
    
    uint64_t file_table_sectors = (sizeof(struct file_table) + sector_size - 1) / sector_size;
    uint64_t file_table_start_sector = sb->file_table_start_block * sectors_per_block;

    serial_printf("\nWriting file table to sector %llu (%llu sectors)...\n", 
                  file_table_start_sector, file_table_sectors);

    if (ide_write_sectors(drive_num, file_table_sectors, file_table_start_sector, ft) != 0) {
        serial_printf("Failed to write file table!\n");
        destroy_file_table(ft);
        destroy_super_block(sb);
        return FAILED_TO_WRITE_FILE_TABLE;
    }

    serial_printf("File table written successfully\n");
    
    struct file_table* verify_ft = kmalloc(sizeof(struct file_table));
    if (!verify_ft) {
        destroy_file_table(ft);
        destroy_super_block(sb);
        return FAILED_TO_READ_FILE_TABLE;
    }
    
    serial_printf("\nReading file table verification:\n");
    if (ide_read_sectors(drive_num, file_table_sectors, file_table_start_sector, verify_ft) != 0) {
        serial_printf("Failed to read file table for verification!\n");
        kfree(verify_ft);
        destroy_file_table(ft);
        destroy_super_block(sb);
        return FAILED_TO_READ_FILE_TABLE;
    }
    
    serial_printf("  Magic: 0x%x (expected 0x454C4958)\n", verify_ft->magic);
    serial_printf("  Max entries: %u\n", verify_ft->max_entries);
    serial_printf("  Num entries: %u\n", verify_ft->num_entries);
    serial_printf("  Reserved: %u\n", verify_ft->reserved);
    
    if (verify_ft->magic != FILE_TABLE_MAGIC) {
        serial_printf("ERROR: File table magic mismatch!\n");
        kfree(verify_ft);
        destroy_file_table(ft);
        destroy_super_block(sb);
        return FAILED_FILE_TABLE_VERIFICATION;
    }
    
    kfree(verify_ft);
    destroy_file_table(ft);
    destroy_super_block(sb);
    
    serial_printf("\n=== Elixir filesystem initialized successfully ===\n");
    return SUCCESS;
}
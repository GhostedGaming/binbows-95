#include <elixir.h>
#include <serial.h>
#include <util.h>
#include <mem.h>
#include <sata.h>

struct elixir_init_data* init_elixir(HBA_PORT *port) {
    uint16_t *identify_buf = (uint16_t *)kmalloc(512);
    if (!identify_buf) {
        serial_printf("Failed to allocate identify buffer!\n");
        return NULL;
    }
    
    uint32_t identify_words[256];
    for (int i = 0; i < 256; i++) {
        identify_words[i] = identify_buf[i];
    }
    
    if (!ahci_identify(port, identify_buf)) {
        serial_printf("Failed to identify drive!\n");
        kfree(identify_buf);
        return NULL;
    }
    
    uint64_t total_sectors = *(uint32_t *)&identify_buf[60];
    if (identify_buf[83] & (1 << 10)) {
        total_sectors = *(uint64_t *)&identify_buf[100];
    }
    
    kfree(identify_buf);
    
    uint32_t block_size = 4096;
    uint32_t sector_size = 512;
    uint8_t sectors_per_block = block_size / sector_size;
    
    uint64_t total_blocks = (total_sectors * sector_size) / block_size;
    
    serial_printf("Initializing Elixir filesystem...\n");
    serial_printf("  Total sectors: %llu\n", total_sectors);
    serial_printf("  Total blocks: %llu\n", total_blocks);
    serial_printf("  Sectors per block: %u\n", sectors_per_block);

    // Allocate the init data structure
    struct elixir_init_data* init_data = kmalloc(sizeof(struct elixir_init_data));
    if (!init_data) {
        serial_printf("Failed to allocate init data!\n");
        return NULL;
    }
    
    init_data->sectors_per_block = sectors_per_block;

    // Create superblock
    struct super_block* sb = create_super_block(total_blocks, block_size);
    if (!sb) {
        serial_printf("Failed to create superblock!\n");
        kfree(init_data);
        return NULL;
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
        kfree(init_data);
        return NULL;
    }

    // Create file table
    serial_printf("\nCreating file table...\n");
    struct file_table* ft = create_file_table();
    if (!ft) {
        serial_printf("Failed to allocate file table!\n");
        destroy_super_block(sb);
        kfree(init_data);
        return NULL;
    }
    
    serial_printf("File table created:\n");
    serial_printf("  Size: %llu bytes\n", (uint64_t)sizeof(struct file_table));
    serial_printf("  Magic: 0x%x\n", ft->magic);
    serial_printf("  Max entries: %u\n", ft->max_entries);
    serial_printf("  Num entries: %u\n", ft->num_entries);
    
    uint64_t file_table_sectors = (sizeof(struct file_table) + sector_size - 1) / sector_size;
    uint64_t file_table_start_sector = sb->file_table_start_block * sectors_per_block;

    serial_printf("\nFile table will be at sector %llu (%llu sectors)\n", 
                  file_table_start_sector, file_table_sectors);
    
    init_data->sb = sb;
    init_data->ft = ft;
    init_data->file_table_start_sector = file_table_start_sector;
    init_data->file_table_sectors = file_table_sectors;
    
    serial_printf("\n=== Elixir filesystem structures created successfully ===\n");
    serial_printf("Use write_elixir_to_disk() to write structures to disk\n");
    
    return init_data;
}

int write_elixir_to_disk(HBA_PORT *port, struct elixir_init_data* init_data) {
    if (!init_data || !init_data->sb || !init_data->ft) {
        serial_printf("Invalid init_data!\n");
        return -1;
    }
    
    serial_printf("\nWriting superblock to sector 0...\n");
    if (!write_sectors(port, 0, 0, init_data->sectors_per_block, init_data->sb)) {
        serial_printf("Failed to write superblock!\n");
        return -2;
    }
    serial_printf("Superblock written successfully\n");
    
    struct super_block* verify_sb = kmalloc(sizeof(struct super_block));
    if (!verify_sb) {
        return -3;
    }
    
    serial_printf("\nReading superblock verification:\n");
    if (!read_sectors(port, 0, 0, init_data->sectors_per_block, verify_sb)) {
        serial_printf("Failed to read superblock for verification!\n");
        kfree(verify_sb);
        return -4;
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

    // Write file table
    serial_printf("\nWriting file table to sector %llu (%llu sectors)...\n", 
                  init_data->file_table_start_sector, init_data->file_table_sectors);

    // Split the sector address into low and high parts
    uint64_t sector_addr = init_data->file_table_start_sector;
    uint32_t startl = (uint32_t)(sector_addr & 0xFFFFFFFF);
    uint32_t starth = (uint32_t)(sector_addr >> 32);
    
    if (!write_sectors(port, startl, starth, init_data->file_table_sectors, init_data->ft)) {
        serial_printf("Failed to write file table!\n");
        return -5;
    }

    serial_printf("File table written successfully\n");
    
    // Verify file table
    struct file_table* verify_ft = kmalloc(sizeof(struct file_table));
    if (!verify_ft) {
        return -6;
    }
    
    serial_printf("\nReading file table verification:\n");
    if (!read_sectors(port, startl, starth, init_data->file_table_sectors, verify_ft)) {
        serial_printf("Failed to read file table for verification!\n");
        kfree(verify_ft);
        return -7;
    }
    
    serial_printf("  Magic: 0x%x (expected 0x454C4958)\n", verify_ft->magic);
    serial_printf("  Max entries: %u\n", verify_ft->max_entries);
    serial_printf("  Num entries: %u\n", verify_ft->num_entries);
    serial_printf("  Reserved: %u\n", verify_ft->reserved);
    
    if (verify_ft->magic != FILE_TABLE_MAGIC) {
        serial_printf("ERROR: File table magic mismatch!\n");
        kfree(verify_ft);
        return -8;
    }
    
    kfree(verify_ft);
    
    serial_printf("\n=== Elixir filesystem written to disk successfully ===\n");
    return 0;
}

void destroy_elixir_init_data(struct elixir_init_data* init_data) {
    if (init_data) {
        if (init_data->sb) {
            destroy_super_block(init_data->sb);
        }
        if (init_data->ft) {
            destroy_file_table(init_data->ft);
        }
        kfree(init_data);
    }
}
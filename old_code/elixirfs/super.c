#include <elixir.h>
#include <util.h>
#include <mem.h>

struct super_block* create_super_block(uint64_t total_blocks, uint32_t block_size) {
    struct super_block* sb = kmalloc(sizeof(struct super_block));
    
    if (!sb) {
        return NULL;
    }
    
    memset(sb, 0, sizeof(struct super_block));
    
    sb->magic = SUPER_BLOCK_MAGIC;
    sb->version = 1;
    sb->total_blocks = total_blocks;
    sb->block_size = block_size;
    
    sb->file_table_start_block = 1;
    
    uint64_t file_table_bytes = sizeof(struct file_table);
    sb->file_table_size_blocks = (file_table_bytes + block_size - 1) / block_size;
    
    sb->data_start_block = 1 + sb->file_table_size_blocks;
    
    sb->free_blocks = total_blocks - 1 - sb->file_table_size_blocks;
    
    return sb;
}

void destroy_super_block(struct super_block* sb) {
    if (sb) {
        kfree(sb);
    }
}
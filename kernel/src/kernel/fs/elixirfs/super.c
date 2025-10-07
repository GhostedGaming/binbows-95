#include <stdint.h>

#include <elixir.h>
#include <util.h>

struct super_block create_super_block(uint64_t total_blocks, uint32_t block_size) {
    struct super_block sb;
    memset(&sb, 0, sizeof(struct super_block));
    
    sb.magic_number = hash_string("where am I?");
    sb.version = 1;
    sb.total_blocks = total_blocks;
    sb.block_size = block_size;
    sb.root_dir_block = 1;
    sb.free_blocks = total_blocks - 2;
    
    return sb;
}
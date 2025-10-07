#ifndef ELIXIR_H
#define ELIXIR_H

#include <stdint.h>

struct super_block {
    uint32_t magic_number;
    uint32_t version;
    uint64_t total_blocks;
    uint32_t block_size;
    uint64_t root_dir_block;
    uint32_t free_blocks;
    uint8_t reserved[512];
};

struct super_block create_super_block(uint64_t total_blocks, uint32_t block_size);
void init_elixir();

#endif
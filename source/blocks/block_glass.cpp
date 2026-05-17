#include "block_glass.hpp"

BlockGlass::BlockGlass(uint16_t id, uint8_t texture_index, Materials material) : BlockShatterable(id, texture_index, material)
{
}

uint16_t BlockGlass::drop_count(javaport::Random &random)
{
    return 0;
}
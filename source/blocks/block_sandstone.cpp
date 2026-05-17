#include "block_sandstone.hpp"

BlockSandStone::BlockSandStone(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
}

uint8_t BlockSandStone::face_texture_index(uint8_t face, uint8_t meta)
{
    if (face == +BlockFace::NY)
        return 208;
    if (face == +BlockFace::PY)
        return 176;
    return 192;
}
#include "block_wool.hpp"

BlockWool::BlockWool(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::CLOTH)
{
    data.sound_type = BlockSoundType::cloth;
}

uint16_t BlockWool::drop_meta(uint16_t meta)
{
    return meta;
}

uint8_t BlockWool::face_texture_index(uint8_t face, uint8_t meta)
{
    if (meta == 0)
        return 64;

    meta = 15 - meta;
    return 113 + ((meta & 8) >> 3) + ((meta & 7) << 4);
}
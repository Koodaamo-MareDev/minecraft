#include "block_bookshelf.hpp"

BlockBookshelf::BlockBookshelf(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::WOOD)
{
    data.sound_type = BlockSoundType::wood;
}

uint8_t BlockBookshelf::texture_index(World *world, const Vec3i &pos, uint8_t face)
{
    return face == +BlockFace::NY || face == +BlockFace::PY ? 4 : 35;
}

uint16_t BlockBookshelf::drop_count(javaport::Random &random)
{
    return 0;
}

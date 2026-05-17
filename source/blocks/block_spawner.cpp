#include "block_spawner.hpp"

BlockSpawner::BlockSpawner(uint16_t id, uint8_t texture_index) : BlockContainer(id, texture_index, Materials::ROCK)
{
    data.sound_type = BlockSoundType::metal;
}

bool BlockSpawner::is_opaque()
{
    return false;
}

uint16_t BlockSpawner::drop_id(uint16_t meta, javaport::Random &random)
{
    return 0;
}

uint16_t BlockSpawner::drop_count(javaport::Random &random)
{
    return 0;
}

TileEntity *BlockSpawner::get_tile_entity(World *world, const Vec3i &pos)
{
    // TODO: Create spawner tile entity
    return nullptr;
}

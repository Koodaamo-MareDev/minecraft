#include "block_shatterable.hpp"
#include <world/world.hpp>

BlockShatterable::BlockShatterable(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
    data.sound_type = BlockSoundType::glass;
}

bool BlockShatterable::is_opaque()
{
    return false;
}

bool BlockShatterable::should_render_side(World *world, const Vec3i &pos, uint8_t face)
{
    BlockID block_id = world->get_block_id_at(pos);
    return block_id != data.id && BlockBase::should_render_side(world, pos, face);
}

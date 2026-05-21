#include "block_shatterable.hpp"
#include <world/world.hpp>
#include <registry/block_list.hpp>

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
    BlockID block_id = world->get_block_id_at(pos + block_face[face]);
    if (block_id == data.id)
        return false;
    return BlockBase::should_render_side(world, pos, face);
}

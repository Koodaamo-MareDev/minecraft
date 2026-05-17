#include "block_fence.hpp"

#include <world/world.hpp>

BlockFence::BlockFence(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::WOOD)
{
    data.sound_type = BlockSoundType::wood;
    data.render_type = BlockRenderType::special;
    data.aabb = AABB(Vec3f(0.0, 0.0, 0.0), Vec3f(1.0, 1.5, 1.0));
}

bool BlockFence::can_place(World *world, const Vec3i &pos)
{
    // Can't place fences on top of another fence.
    if (world->get_block_at({pos.x, pos.y - 1, pos.z})->id == data.block_id)
        return false;

    return false;
}

void BlockFence::get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    aabb.min = Vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + Vec3f(1, 1, 1);
    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

bool BlockFence::is_opaque()
{
    return false;
}
#include "block_ice.hpp"

#include <world/world.hpp>
#include <registry/block_list.hpp>

BlockIce::BlockIce(uint16_t id, uint8_t texture_index, Materials material) : BlockShatterable(id, texture_index, material)
{
    data.slipperiness = 0.98f;
    data.tick_on_load = true;
}

bool BlockIce::should_render_side(World *world, const Vec3i &pos, uint8_t face)
{
    return BlockShatterable::should_render_side(world, pos, face);
}

void BlockIce::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    BlockState *block = world->get_block_at(pos);
    if (block->block_light > 11 - data.light_opacity)
        world->set_block_at(pos, BlockID::flowing_water);
}

uint16_t BlockIce::drop_count(javaport::Random &random)
{
    return 0;
}

void BlockIce::on_removed(World *world, const Vec3i &pos)
{
    BlockBase *block = block_at(world, pos - Vec3i(0, 1, 0));
    if (block->is_opaque() || block->material().is_liquid)
    {
        world->set_block_at(pos, BlockID::water);
        world->notify_at(pos);
    }
}

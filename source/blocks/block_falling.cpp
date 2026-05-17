#include "block_falling.hpp"

#include <world/world.hpp>
#include <world/entity.hpp>
#include <block/block_list.hpp>

BlockFalling::BlockFalling(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
}

void BlockFalling::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    Block *block = world->get_block_at(pos + Vec3i{0, -1, 0});

    if (block->id == 0 || block_list[block->id]->material().is_liquid)
    {
        world->add_entity(new EntityFallingBlock(*block, pos));
        world->set_block_and_meta_at(pos, BlockID::air, 0);
        world->notify_at(pos);
    }
}

void BlockFalling::on_added(World *world, const Vec3i &pos)
{
    world->schedule_block_update(pos, data.block_id, 3);
}

void BlockFalling::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    world->schedule_block_update(pos, data.block_id, 3);
}

#include "block_reeds.hpp"

#include <render/render_blocks.hpp>
#include <world/world.hpp>
#include <registry/block_list.hpp>

BlockReeds::BlockReeds(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::PLANTS)
{
    data.aabb = AABB(Vec3f(2.0 / 16.0, 0.0, 2.0 / 16.0), Vec3f(14.0 / 16.0, 1.0, 14.0 / 16.0));
    data.tick_on_load = true;
    data.render_type = BlockRenderType::cross;
    data.render_func = render_cross;
}

bool BlockReeds::can_place(World *world, const Vec3i &pos)
{
    BlockID below = world->get_block_id_at({pos.x, pos.y - 1, pos.z});
    if (below == data.block_id)
        return true;
    if (below != BlockID::dirt && below != BlockID::grass)
        return false;
    if (block_at(world, pos + Vec3i{-1, -1, 0})->material_type() == Materials::WATER ||
        block_at(world, pos + Vec3i{1, -1, 0})->material_type() == Materials::WATER ||
        block_at(world, pos + Vec3i{0, -1, -1})->material_type() == Materials::WATER ||
        block_at(world, pos + Vec3i{0, -1, 1})->material_type() == Materials::WATER)
        return true;
    return false;
}

bool BlockReeds::can_stay(World *world, const Vec3i &pos)
{
    return can_place(world, pos);
}

void BlockReeds::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    if (can_stay(world, pos))
        return;
    drop_item(world, pos, world->get_meta_at(pos));
    world->set_block_at(pos, BlockID::air);
    world->notify_at(pos);
}

void BlockReeds::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    if (world->get_block_id_at(pos + Vec3i{0, 1, 0}) == BlockID::air)
    {
        uint8_t height = 1;
        while (world->get_block_id_at(pos + Vec3i{0, -height, 0}) == data.block_id)
            height++;
        if (height < 3)
        {
            uint8_t age = world->get_meta_at(pos);
            if (age == 15)
            {
                world->set_block_at(pos + Vec3i{0, 1, 0}, data.block_id);
                world->set_meta_at(pos, 0);
            }
            else
            {
                world->set_meta_at(pos, age + 1);
            }
        }
    }
}

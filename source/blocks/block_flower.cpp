#include "block_flower.hpp"
#include <world/world.hpp>

BlockFlower::BlockFlower(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
    data.sound_type = BlockSoundType::grass;
    data.render_type = BlockRenderType::cross;
    data.tick_on_load = true;
    data.aabb = AABB(Vec3f(0.3, 0.0, 0.3), Vec3f(0.7, 0.6, 0.7));
}

BlockFlower::BlockFlower(uint16_t id, uint8_t texture_index) : BlockFlower(id, texture_index, Materials::WOOD)
{
    data.sound_type = BlockSoundType::grass;
    data.render_type = BlockRenderType::cross;
    data.tick_on_load = true;
    data.aabb = AABB(Vec3f(0.3, 0.0, 0.3), Vec3f(0.7, 0.6, 0.7));
}

bool BlockFlower::can_place(World *world, const Vec3i &pos)
{
    return can_grow_on(world->get_block_id_at({pos.x, pos.y - 1, pos.z}));
}

bool BlockFlower::can_stay(World *world, const Vec3i &pos)
{
    Block *block = world->get_block_at(pos);
    return (block->block_light >= 8 || block->sky_light == 15) &&
           can_grow_on(world->get_block_id_at({pos.x, pos.y - 1, pos.z}));
}

void BlockFlower::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    check_can_stay(world, pos);
}

void BlockFlower::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    check_can_stay(world, pos);
}

bool BlockFlower::is_opaque()
{
    return false;
}

bool BlockFlower::can_grow_on(uint8_t block_id)
{
    return block_id == +BlockID::grass || block_id == +BlockID::dirt || block_id == +BlockID::farmland;
}

void BlockFlower::check_can_stay(World *world, const Vec3i &pos)
{
    if (!can_stay(world, pos))
    {
        drop_item(world, pos, world->get_meta_at(pos));
        world->set_block_at(pos, BlockID::air);
    }
}

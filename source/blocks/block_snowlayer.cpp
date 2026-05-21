#include "block_snowlayer.hpp"

#include <render/render_blocks.hpp>
#include <world/world.hpp>
#include <registry/block_list.hpp>
#include <item/item_stack.hpp>
#include <item/item_id.hpp>

BlockSnowLayer::BlockSnowLayer(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
    data.tick_on_load = true;
    data.sound_type = BlockSoundType::cloth;
    data.render_func = render_snow_layer;
    data.aabb = AABB(Vec3f(0.0, 0.0, 0.0), Vec3f(1.0, 2.0 / 16.0, 1.0));
}

bool BlockSnowLayer::is_opaque()
{
    return false;
}

bool BlockSnowLayer::should_render_side(World *world, const Vec3i &pos, uint8_t face)
{
    if (face == +BlockFace::PY)
        return true;
    return BlockBase::should_render_side(world, pos, face);
}

bool BlockSnowLayer::can_place(World *world, const Vec3i &pos)
{
    BlockBase *below = block_at(world, pos + Vec3i{0, -1, 0});
    return below->is_opaque() && below->material().is_solid;
}

bool BlockSnowLayer::can_stay(World *world, const Vec3i &pos)
{
    return can_place(world, pos);
}

void BlockSnowLayer::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    if (!can_stay(world, pos))
        world->destroy_block(pos, world->get_block_at(pos));
}

void BlockSnowLayer::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    BlockState block = *world->get_block_at(pos);
    if (block.block_light > 11)
        world->destroy_block(pos, &block);
}

void BlockSnowLayer::on_harvest(World *world, const Vec3i &pos, uint8_t meta)
{
    item::ItemStack snowball = item::ItemStack(+ItemID::snowball, 1);
    world->spawn_drop(pos, snowball);
    world->destroy_block(pos, world->get_block_at(pos));
}

uint16_t BlockSnowLayer::drop_id(uint16_t meta, javaport::Random &random)
{
    return 0;
}

uint16_t BlockSnowLayer::drop_count(javaport::Random &random)
{
    return 0;
}

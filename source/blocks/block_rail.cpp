#include "block_rail.hpp"

#include <world/world.hpp>
#include <block/block_list.hpp>

BlockRail::BlockRail(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::CIRCUITS)
{
    data.sound_type = BlockSoundType::metal;
}

uint8_t BlockRail::face_texture_index(uint8_t face, uint8_t meta)
{
    return 0;
}

bool BlockRail::is_opaque()
{
    return false;
}

void BlockRail::get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    uint8_t meta = world->get_meta_at(pos);
    float height = (meta >= 2 && meta <= 5) ? 10 : 2;
    AABB bbox(Vec3f(pos.x, pos.y, pos.z), Vec3f(pos.x + 1, pos.y + height / 16.0f, pos.z + 1));
    if (bbox.intersects(other))
        aabb_list.push_back(bbox);
}

bool BlockRail::can_place(World *world, const Vec3i &pos)
{
    return block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque();
}

void BlockRail::on_added(World *world, const Vec3i &pos)
{
    if (!world->is_remote())
    {
        world->set_meta_at(pos, 15);
        resolve_state(world, pos);
    }
}

void BlockRail::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    if (!world->is_remote())
    {
        uint8_t meta = world->get_meta_at(pos);

        bool should_break = false;

        if (!block_at(world, {pos.x, pos.y - 1, pos.z})->is_opaque())
            should_break = true;
        if (meta == 2 && !block_at(world, {pos.x + 1, pos.y, pos.z})->is_opaque())
            should_break = true;
        if (meta == 3 && !block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque())
            should_break = true;
        if (meta == 4 && !block_at(world, {pos.x, pos.y, pos.z + 1})->is_opaque())
            should_break = true;
        if (meta == 5 && !block_at(world, {pos.x, pos.y, pos.z - 1})->is_opaque())
            should_break = true;

        if (should_break)
            world->destroy_block(pos, world->get_block_at(pos));
        else
        {
            resolve_state(world, pos);
        }
    }
}

void BlockRail::resolve_state(World *world, const Vec3i &pos)
{
    // TODO: resolve state
}
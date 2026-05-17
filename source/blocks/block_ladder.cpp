#include "block_ladder.hpp"

#include <world/world.hpp>
#include <block/block_list.hpp>

BlockLadder::BlockLadder(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::CIRCUITS)
{
}

void BlockLadder::get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    uint8_t meta = world->get_meta_at(pos);

    float width = 2.0f;
    AABB bbox(Vec3f(pos.x, pos.y, pos.z), Vec3f(pos.x + 1, pos.y + 1, pos.z + 1));

    switch (meta)
    {
    case 2:
        bbox.min.z = bbox.max.z - width;
        break;
    case 3:
        bbox.max.z = bbox.min.z + width;
        break;
    case 4:
        bbox.min.x = bbox.max.x - width;
        break;
    case 5:
        bbox.max.x = bbox.min.x + width;
        break;
    default:
        // Full block - shouldn't be the case
        break;
    }
    if (bbox.intersects(other))
        aabb_list.push_back(bbox);
}

bool BlockLadder::can_place(World *world, const Vec3i &pos)
{
    return (block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque() ||
            block_at(world, {pos.x + 1, pos.y, pos.z})->is_opaque() ||
            block_at(world, {pos.x, pos.y, pos.z - 1})->is_opaque() ||
            block_at(world, {pos.x, pos.y, pos.z + 1})->is_opaque());
}

void BlockLadder::on_placed(World *world, const Vec3i &pos, uint8_t face)
{
    uint8_t meta = world->get_meta_at(pos);

    if ((meta == 0 || face == +BlockFace::NZ) && block_at(world, {pos.x, pos.y, pos.z + 1})->is_opaque())
        meta = 2;
    if ((meta == 0 || face == +BlockFace::PZ) && block_at(world, {pos.x, pos.y, pos.z - 1})->is_opaque())
        meta = 3;
    if ((meta == 0 || face == +BlockFace::NX) && block_at(world, {pos.x + 1, pos.y, pos.z})->is_opaque())
        meta = 4;
    if ((meta == 0 || face == +BlockFace::PX) && block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque())
        meta = 5;

    world->set_meta_at(pos, meta);
}

void BlockLadder::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    uint8_t meta = world->get_meta_at(pos);

    bool should_break = false;

    if (meta == 2 && block_at(world, {pos.x, pos.y, pos.z + 1})->is_opaque())
        should_break = true;
    if (meta == 3 && block_at(world, {pos.x, pos.y, pos.z - 1})->is_opaque())
        should_break = true;
    if (meta == 4 && block_at(world, {pos.x + 1, pos.y, pos.z})->is_opaque())
        should_break = true;
    if (meta == 5 && block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque())
        should_break = true;

    if (should_break)
        world->destroy_block(pos, world->get_block_at(pos));
}

#include "block_torch.hpp"

#include <render/render_blocks.hpp>
#include <world/world.hpp>
#include <registry/block_list.hpp>

BlockTorch::BlockTorch(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::WOOD)
{
    data.render_type = BlockRenderType::special;
    data.render_func = render_torch;
}

bool BlockTorch::is_opaque()
{
    return false;
}

void BlockTorch::get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
}

void BlockTorch::get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    BlockState *block = world->get_block_at(pos);
    AABB aabb;
    constexpr vfloat_t offset = 0.35;
    constexpr vfloat_t width = 0.3;
    constexpr vfloat_t height = 0.6;
    aabb.min = Vec3f(pos.x + offset, pos.y + 0.2, pos.z + offset);
    aabb.max = aabb.min + Vec3f(width, height, width);
    switch (block->meta)
    {
    case 1:
        aabb.translate(Vec3f(-offset, 0, 0));
        break;
    case 2:
        aabb.translate(Vec3f(offset, 0, 0));
        break;
    case 3:
        aabb.translate(Vec3f(0, 0, -offset));
        break;
    case 4:
        aabb.translate(Vec3f(0, 0, offset));
        break;
    default:
        aabb.translate(Vec3f(0, -0.2, 0));
        break;
    }

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void BlockTorch::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    if (world->get_meta_at(pos) == 0)
        on_added(world, pos);
}

bool BlockTorch::can_stay(World *world, const Vec3i &pos)
{
    for (int i = 0; i < 6; i++)
    {
        if (i == +BlockFace::PY)
            continue;
        if (block_at(world, pos + block_face[i])->is_opaque())
            return true;
    }
    return false;
}

void BlockTorch::on_placed(World *world, const Vec3i &pos, uint8_t face)
{
    uint8_t meta = world->get_meta_at(pos);

    if (face == +BlockFace::PY && block_at(world, pos + Vec3i{0, -1, 0})->is_opaque())
        meta = 5;
    else if (face == 2 && block_at(world, pos + Vec3i{0, 0, 1})->is_opaque())
        meta = 4;
    else if (face == 3 && block_at(world, pos + Vec3i{0, 0, -1})->is_opaque())
        meta = 3;
    else if (face == 4 && block_at(world, pos + Vec3i{-1, 0, 0})->is_opaque())
        meta = 2;
    else if (face == 5 && block_at(world, pos + Vec3i{1, 0, 0})->is_opaque())
        meta = 1;
    else
        meta = 0;

    world->set_meta_at(pos, meta);
}

void BlockTorch::on_added(World *world, const Vec3i &pos)
{
    if (block_at(world, pos + Vec3i{-1, 0, 0})->is_opaque())
        world->set_meta_at(pos, 1);
    else if (block_at(world, pos + Vec3i{1, 0, 0})->is_opaque())
        world->set_meta_at(pos, 2);
    else if (block_at(world, pos + Vec3i{0, 0, -1})->is_opaque())
        world->set_meta_at(pos, 3);
    else if (block_at(world, pos + Vec3i{0, 0, 1})->is_opaque())
        world->set_meta_at(pos, 4);
    else if (block_at(world, pos + Vec3i{0, -1, 0})->is_opaque())
        world->set_meta_at(pos, 5);

    if (!can_stay(world, pos))
        world->destroy_block(pos, world->get_block_at(pos));
}

void BlockTorch::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    if (can_stay(world, pos))
        return;

    uint8_t meta = world->get_meta_at(pos);

    if ((meta == 1 && !block_at(world, pos + Vec3i{-1, 0, 0})->is_opaque()) ||
        (meta == 2 && !block_at(world, pos + Vec3i{+1, 0, 0})->is_opaque()) ||
        (meta == 3 && !block_at(world, pos + Vec3i{0, 0, -1})->is_opaque()) ||
        (meta == 4 && !block_at(world, pos + Vec3i{0, 0, +1})->is_opaque()) ||
        (meta == 5 && !block_at(world, pos + Vec3i{0, -1, 0})->is_opaque()))
        world->destroy_block(pos, world->get_block_at(pos));
}

#include "block_slab.hpp"

#include <world/world.hpp>
#include <render/render_blocks.hpp>
#include <registry/block_list.hpp>

BlockSlab::BlockSlab(uint16_t id, uint8_t texture_index, bool is_double_slab) : BlockBase(id, texture_index, Materials::ROCK), is_double_slab(is_double_slab)
{
    data.render_func = is_double_slab ? render_cube : render_slab;
}

bool BlockSlab::is_opaque()
{
    return is_double_slab;
}

bool BlockSlab::should_render_side(World *world, const Vec3i &pos, uint8_t face)
{
    BlockBase *b = block_at(world, pos + block_face[face]);
    std::vector<AABB> aabbs;
    b->get_colliding_aabb(world, pos + block_face[face], aabb().extend_by(Vec3f(0.5, 0.5, 0.5)), aabbs);
    AABB bbox = aabbs.empty() ? b->aabb() : aabbs[0];
    if (face == +BlockFace::NX && bbox.min.x > 0.0)
        return true;
    if (face == +BlockFace::PX && bbox.max.x < 1.0)
        return true;
    if (face == +BlockFace::NY && bbox.min.y > 0.0)
        return true;
    if (face == +BlockFace::PY && bbox.max.y < 1.0)
        return true;
    if (face == +BlockFace::NZ && bbox.min.z > 0.0)
        return true;
    if (face == +BlockFace::PZ && bbox.max.z < 1.0)
        return true;
    return !b->is_opaque();
}

uint8_t BlockSlab::face_texture_index(uint8_t face, uint8_t meta)
{
    int variant = meta & 7;
    switch (variant)
    {
    case 0: // Stone
    {
        if (face == +BlockFace::NY || face == +BlockFace::PY)
            return 6;
        return 5;
    }
    case 1: // Sandstone
    {
        if (face == +BlockFace::NY)
            return 208;
        if (face == +BlockFace::PY)
            return 176;
        return 192;
    }
    case 2: // Wooden
        return 4;
    case 3: // Cobblestone
        return 16;
    default:
        return 6;
    }
}

uint16_t BlockSlab::drop_meta(uint16_t meta)
{
    return meta & 7;
}

uint16_t BlockSlab::drop_count(javaport::Random &random)
{
    return is_double_slab ? 2 : 1;
}

void BlockSlab::get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    BlockState *block = world->get_block_at(pos);
    AABB aabb;

    aabb.min = Vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + Vec3f(1, 1, 1);

    if (!is_double_slab)
    {
        if ((block->meta & 8))
        {
            aabb.min.y = 0.5;
        }
        else
        {
            aabb.max.y = 0.5;
        }
    }

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

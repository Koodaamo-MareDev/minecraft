#include "block_sign.hpp"

#include <block/block_list.hpp>
#include <item/item_id.hpp>
#include <world/world.hpp>
#include <world/tile_entity/tile_entity_sign.hpp>

BlockSign::BlockSign(uint16_t id, uint8_t texture_index, bool is_post) : BlockContainer(id, texture_index, Materials::WOOD)
{
    is_post = is_post;
    data.render_type = BlockRenderType::special;
    data.aabb = AABB(Vec3f(0.25, 0.0, 0.25), Vec3f(0.75, 1.0, 0.75));
}

bool BlockSign::is_opaque()
{
    return false;
}

void BlockSign::get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB bbox = data.aabb;
    if (!is_post)
    {
        uint8_t facing = world->get_meta_at(pos);
        switch (facing)
        {
        case 2:
        {
            bbox.min.x = 0.0;
            bbox.max.x = 1.0;
            bbox.min.z = 14.0 / 16.0;
            bbox.max.z = 1.0;
            break;
        }
        case 3:
        {
            bbox.min.x = 0.0;
            bbox.max.x = 1.0;
            bbox.min.z = 0.0;
            bbox.max.z = 2.0 / 16.0;
            break;
        }
        case 4:
        {
            bbox.min.x = 14.0 / 16.0;
            bbox.max.x = 1.0;
            bbox.min.z = 0.0;
            bbox.max.z = 1.0;
            break;
        }
        case 5:
        {
            bbox.min.x = 0.0;
            bbox.max.x = 2.0 / 16.0;
            bbox.min.z = 0.0;
            bbox.max.z = 1.0;
            break;
        }
        }
        bbox.min.y = 9.0 / 32.0;
        bbox.max.y = 25.0 / 32.0;
    }
    bbox.translate(Vec3f(pos.x, pos.y, pos.z));
    if (bbox.intersects(other))
        aabb_list.push_back(bbox);
}

TileEntity *BlockSign::get_tile_entity(World *world, const Vec3i &pos)
{
    return new TileEntitySign;
}

uint16_t BlockSign::drop_id(uint16_t meta, javaport::Random &random)
{
    return +ItemID::sign;
}

void BlockSign::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    bool should_break = false;

    if (is_post)
    {
        if (!block_at(world, pos + Vec3i{0, -1, 0})->material().is_solid)
        {
            should_break = true;
        }
    }
    else
    {
        // Wall sign cases
        uint8_t facing = world->get_meta_at(pos);
        switch (facing)
        {
        case 2:
        {
            if (!block_at(world, pos + Vec3i{0, 0, 1})->material().is_solid)
                should_break = true;
            break;
        }
        case 3:
        {
            if (!block_at(world, pos + Vec3i{0, 0, -1})->material().is_solid)
                should_break = true;
            break;
        }
        case 4:
        {
            if (!block_at(world, pos + Vec3i{-1, 0, 0})->material().is_solid)
                should_break = true;
            break;
        }
        case 5:
        {
            if (!block_at(world, pos + Vec3i{1, 0, 0})->material().is_solid)
                should_break = true;
            break;
        }
        }
    }
    if (should_break)
    {
        BlockState old_block = *world->get_block_at(pos);
        world->destroy_block(pos, &old_block);
    }
}

#include "block_soil.hpp"

#include <world/world.hpp>
#include <world/entity.hpp>
#include <registry/block_list.hpp>

BlockSoil::BlockSoil(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::GROUND)
{
    data.tick_on_load = true;
    data.aabb = AABB(Vec3f(0.0, 0.0, 0.0), Vec3f(1.0, 15.0 / 16.0, 1.0));
    data.light_opacity = 255;
}

uint8_t BlockSoil::face_texture_index(uint8_t face, uint8_t meta)
{
    if (face == +BlockFace::PY)
        return meta > 0 ? data.texture_index - 1 : data.texture_index;
    return 2;
}

void BlockSoil::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    if (random.nextInt(5) == 0)
    {
        if (has_water(world, pos))
        {
            // Set max moisture
            world->set_meta_at(pos, 7);
        }
        else
        {
            // Decrease moisture
            uint8_t moisture = world->get_meta_at(pos);
            if (moisture > 0)
            {
                world->set_meta_at(pos, moisture - 1);
                world->notify_at(pos);
            }
            else if (world->get_block_at(pos)->blockid != BlockID::wheat)
            {
                world->set_block_at(pos, BlockID::dirt);
                world->notify_at(pos);
            }
        }
    }
}

void BlockSoil::on_entity_walk(EntityPhysical *entity, const Vec3i &pos)
{
    if (entity->world->random.nextInt(4) == 0)
    {
        entity->world->set_block_at(pos, BlockID::dirt);
        entity->world->notify_at(pos);
    }
}

void BlockSoil::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    if (block_at(world, pos + Vec3i{0, 1, 0})->material().is_solid)
    {
        world->set_block_at(pos, BlockID::dirt);
        world->notify_at(pos);
    }
}

uint16_t BlockSoil::drop_id(uint16_t meta, javaport::Random &random)
{
    return BlockID::dirt;
}

bool BlockSoil::is_opaque()
{
    return false;
}

bool BlockSoil::has_water(World *world, const Vec3i &pos)
{
    for (int x = -4; x <= 4; x++)
        for (int y = 0; y <= 1; y++)
            for (int z = -4; z <= 4; z++)
                if (block_at(world, pos + Vec3i(x, y, z))->material_type() == Materials::WATER)
                    return true;
    return false;
}
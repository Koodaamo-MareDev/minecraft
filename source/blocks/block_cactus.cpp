#include "block_cactus.hpp"

#include <world/world.hpp>
#include <block/block_list.hpp>

BlockCactus::BlockCactus(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::CACTUS)
{
    float off = 1.0f / 16.0f;
    data.aabb = AABB(Vec3f(off, 0.0f, off), Vec3f(1.0f - off, 1.0f, 1.0f - off));
    data.tick_on_load = true;
    data.sound_type = BlockSoundType::cloth;
    data.render_type = BlockRenderType::special;
}

void BlockCactus::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    if (world->get_block_id_at({pos.x, pos.y + 1, pos.z}) == BlockID::air)
    {
        int height = 1;
        while (true)
        {
            if (world->get_block_id_at({pos.x, pos.y - height, pos.z}) == data.block_id)
                break;

            height++;
        }
        if (height < 3)
        {
            uint8_t age = world->get_meta_at({pos.x, pos.y, pos.z});
            if (age == 15)
            {
                world->set_block_and_meta_at({pos.x, pos.y + 1, pos.z}, data.block_id, 0);
            }
            else
            {
                world->set_meta_at({pos.x, pos.y, pos.z}, age + 1);
            }
        }
    }
}

bool BlockCactus::is_opaque()
{
    return false;
}

bool BlockCactus::can_place(World *world, const Vec3i &pos)
{
    if (!BlockBase::can_place(world, pos))
        return false;

    return can_stay(world, pos);
}

void BlockCactus::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    if (can_stay(world, pos))
        return;
    drop_item(world, pos, world->get_meta_at(pos));
    world->set_block_and_meta_at(pos, BlockID::air, 0);
}

bool BlockCactus::can_stay(World *world, const Vec3i &pos)
{
    if (block_list[+world->get_block_id_at({pos.x - 1, pos.y, pos.z})]->material().is_solid)
        return false;
    else if (block_list[+world->get_block_id_at({pos.x + 1, pos.y, pos.z})]->material().is_solid)
        return false;
    else if (block_list[+world->get_block_id_at({pos.x, pos.y, pos.z - 1})]->material().is_solid)
        return false;
    else if (block_list[+world->get_block_id_at({pos.x, pos.y, pos.z + 1})]->material().is_solid)
        return false;

    BlockID below = world->get_block_id_at({pos.x, pos.y - 1, pos.z});

    return below == data.block_id || below == BlockID::sand;
}

void BlockCactus::on_entity_collide(EntityPhysical *entity, const Vec3i &pos)
{
    EntityLiving *living = dynamic_cast<EntityLiving *>(entity);
    if (living)
        living->hurt(1);
}

uint8_t BlockCactus::face_texture_index(uint8_t face, uint8_t meta)
{
    if (face == +BlockFace::NY)
        return data.texture_index + 1;
    else if (face == +BlockFace::PY)
        return data.texture_index - 1;
    return data.texture_index;
}
#include "block_cake.hpp"

#include <world/world.hpp>
#include <world/entity.hpp>
#include <registry/block_list.hpp>

BlockCake::BlockCake(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::CAKE)
{
    data.tick_on_load = true;
    data.sound_type = BlockSoundType::cloth;
    data.render_type = BlockRenderType::special;

    // Setup bounds
    float off = 1.0f / 16.0f;
    float height = 0.5f;
    AABB ret(Vec3f(off, 0, off), Vec3f(1.0f - off, height, 1.0f - off));
    data.aabb = ret;
}

uint8_t BlockCake::face_texture_index(uint8_t face, uint8_t meta)
{
    if (face == +BlockFace::NY)
        return data.texture_index + 3;
    if (face == +BlockFace::PY)
        return data.texture_index;
    if (meta > 0 && face == +BlockFace::NX)
        return data.texture_index + 2;
    return data.texture_index + 1;
}

bool BlockCake::is_opaque()
{
    return false;
}

void BlockCake::get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    uint8_t meta = world->get_meta_at(pos);
    AABB bbox = data.aabb;
    float eaten = (1 + meta * 2) / 16.0f;
    bbox.translate(Vec3f(pos.x, pos.y, pos.z));
    bbox.min.x += eaten;

    if (bbox.intersects(other))
        aabb_list.push_back(bbox);
}

bool BlockCake::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    on_click(entity, pos);
    return true;
}

void BlockCake::on_click(EntityPhysical *entity, const Vec3i &pos)
{
    EntityPlayer *player = dynamic_cast<EntityPlayer *>(entity);
    if (player)
    {
        World *world = player->world;
        if (player->health < 20)
        {
            player->health = std::min(player->health + 3, 20);

            uint8_t meta = player->world->get_meta_at(pos);
            meta++;

            if (meta >= 6) // TODO: Proper heal
            {
                world->set_block_at(pos, BlockID::air);
            }
            else
            {
                world->set_meta_at(pos, meta);
            }
        }
    }
}

bool BlockCake::can_stay(World *world, const Vec3i &pos)
{
    return block_list[+world->get_block_id_at({pos.x, pos.y - 1, pos.z})]->material().is_solid;
}

bool BlockCake::can_place(World *world, const Vec3i &pos)
{
    if (!BlockBase::can_place(world, pos))
        return false;

    return can_stay(world, pos);
}

void BlockCake::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    if (can_stay(world, pos))
        return;
    world->destroy_block(pos, world->get_block_at(pos));
}

uint16_t BlockCake::drop_count(javaport::Random &random)
{
    return 0;
}

uint16_t BlockCake::drop_id(uint16_t meta, javaport::Random &random)
{
    return 0;
}

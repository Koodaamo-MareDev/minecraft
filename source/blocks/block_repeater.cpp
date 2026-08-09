#include "block_repeater.hpp"

#include <world/world.hpp>
#include <registry/block_list.hpp>
#include <item/item_id.hpp>
#include <render/render_blocks.hpp>

BlockRepeater::BlockRepeater(uint16_t id, bool active) : BlockBase(id, Materials::CIRCUITS), active(active)
{
    data.aabb = AABB(Vec3f(0, 0, 0), Vec3f(1, 0.125, 1));
    data.render_func = render_repeater;
}

bool BlockRepeater::is_opaque()
{
    return false;
}

bool BlockRepeater::can_place(World *world, const Vec3i &pos)
{
    return block_at(world, pos - Vec3i{0, 1, 0})->is_opaque() && BlockBase::can_place(world, pos);
}

bool BlockRepeater::can_stay(World *world, const Vec3i &pos)
{
    return block_at(world, pos - Vec3i{0, 1, 0})->is_opaque() && BlockBase::can_stay(world, pos);
}

uint8_t BlockRepeater::face_texture_index(uint8_t face, uint8_t meta)
{
    if (face == +BlockFace::NY)
        return active ? 99 : 115;
    if (face == +BlockFace::PY)
        return active ? 147 : 131;
    return 5;
}

bool BlockRepeater::should_render_side(World *world, const Vec3i &pos, uint8_t face)
{
    return face != +BlockFace::NY && face != +BlockFace::PY;
}

uint16_t BlockRepeater::drop_id(uint16_t meta, javaport::Random &random)
{
    return +ItemID::diode;
}

void BlockRepeater::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    uint8_t meta = world->get_meta_at(pos);

    bool powered = gets_signal(world, pos, meta);
    if (this->active && !powered)
    {
        world->set_block_and_meta_at(pos, BlockID::unpowered_repeater, meta);
        world->notify_at(pos);
    }
    else if (!this->active)
    {
        world->set_block_and_meta_at(pos, BlockID::powered_repeater, meta);
        world->notify_at(pos);
        if (!powered)
            world->schedule_block_update(pos, BlockID::powered_repeater, meta_to_ticks(meta));
    }
}

void BlockRepeater::on_added(World *world, const Vec3i &pos)
{
    for (uint8_t i = 0; i < 6; i++)
        world->notify_at(pos + block_face[i], data.block_id);
}

void BlockRepeater::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id)
{
    if (!can_stay(world, pos))
    {
        drop_item(world, pos, world->get_meta_at(pos));
        world->set_block_at(pos, BlockID::air);
        world->notify_at(pos);
    }
    else
    {
        uint8_t meta = world->get_meta_at(pos);
        bool powered = gets_signal(world, pos, meta);
        if ((this->active && !powered) || (!this->active && powered))
            world->schedule_block_update(pos, data.block_id, meta_to_ticks(meta));
    }
}

bool BlockRepeater::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    World *world = entity->world;
    uint8_t meta = world->get_meta_at(pos);
    uint8_t ticks = (meta >> 2) & 0x03;
    ticks = (ticks + 1) & 0x03;
    meta = (meta & 0x03) | (ticks << 2);
    world->set_meta_at(pos, meta);
    world->notify_at(pos, data.block_id);

    return true;
}

void BlockRepeater::on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity)
{
    int facing = -int(entity->rotation.y / 90 + 0.5) & 3;
    world->set_meta_at(pos, facing);
    world->notify_at(pos);

    if (gets_signal(world, pos, facing))
        world->schedule_block_update(pos, data.block_id, 1);
}

bool BlockRepeater::provides_power(World *world, const Vec3i &pos, uint8_t face)
{
    if (!this->active)
        return false;
    uint8_t meta = world->get_meta_at(pos) & 3;
    return (meta == 0 && face == +BlockFace::PZ) ||
           (meta == 1 && face == +BlockFace::NX) ||
           (meta == 2 && face == +BlockFace::NZ) ||
           (meta == 3 && face == +BlockFace::PX);
}

bool BlockRepeater::provides_indirect_power(World *world, const Vec3i &pos, uint8_t face)
{
    return provides_power(world, pos, face);
}

bool BlockRepeater::is_power_source()
{
    return false;
}

bool BlockRepeater::gets_signal(World *world, const Vec3i &pos, uint8_t meta)
{
    switch (meta & 3)
    {
    case 0:
        return powers_indirectly(world, pos + block_face[+BlockFace::PZ], +BlockFace::PZ);
    case 1:
        return powers_indirectly(world, pos + block_face[+BlockFace::NX], +BlockFace::NX);
    case 2:
        return powers_indirectly(world, pos + block_face[+BlockFace::NZ], +BlockFace::NZ);
    case 3:
        return powers_indirectly(world, pos + block_face[+BlockFace::PX], +BlockFace::PX);
    default:
        return false;
    }
}

int BlockRepeater::meta_to_ticks(uint8_t meta)
{
    return (((meta >> 2) & 0x03) + 1) << 1;
}

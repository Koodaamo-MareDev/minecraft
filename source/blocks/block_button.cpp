#include "block_button.hpp"

#include <registry/block_list.hpp>
#include <world/world.hpp>
#include <sound.hpp>
#include <sounds.hpp>

BlockButton::BlockButton(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::CIRCUITS)
{
    data.is_power_source = true;
    data.tick_on_load = true;
}

bool BlockButton::is_opaque()
{
    return false;
}

bool BlockButton::can_place(World *world, const Vec3i &pos)
{
    return (block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque() ||
            block_at(world, {pos.x + 1, pos.y, pos.z})->is_opaque() ||
            block_at(world, {pos.x, pos.y, pos.z - 1})->is_opaque() ||
            block_at(world, {pos.x, pos.y, pos.z + 1})->is_opaque());
}

void BlockButton::get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{

    uint8_t meta = world->get_meta_at(pos);

    float x_size = 3 / 16.0f;
    float y_size = 2 / 16.0f;
    float z_size = 1 / 16.0f;
    if (!(meta & 8))
        z_size += z_size;

    float x_off = 0.0f;
    float y_off = 0.5f - y_size;
    float z_off = 0.0f;

    switch (meta & 7)
    {
    case 1:
        z_off = 0.5f - z_size / 2;
        break;
    case 2:
        z_off = 0.5f - z_size / 2;
        x_off = 1.0f - x_size;
        break;
    case 3:
        std::swap(x_size, z_size);
        x_off = 0.5f - x_size / 2;
        break;
    case 4:
        std::swap(x_size, z_size);
        x_off = 0.5f - x_size / 2;
        z_off = 1.0f - z_size;
        break;

    default: // No-op if the state is up/down
        break;
    }

    Vec3f min_pos(pos.x + x_off, pos.y + y_off, pos.z + z_off);
    Vec3f max_pos = min_pos + Vec3f(x_size, y_size, z_size);
    AABB bbox(min_pos, max_pos);

    if (bbox.intersects(other))
        aabb_list.push_back(bbox);
}

void BlockButton::on_placed(World *world, const Vec3i &pos, uint8_t face)
{
    uint8_t meta = world->get_meta_at(pos);
    bool pressed = (meta & 8) != 0;
    meta &= 7;
    if (face == +BlockFace::PX && block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque())
    {
        meta = 1;
    }
    else if (face == +BlockFace::NX && block_at(world, {pos.x + 1, pos.y, pos.z})->is_opaque())
    {
        meta = 2;
    }
    else if (face == +BlockFace::PZ && block_at(world, {pos.x, pos.y, pos.z - 1})->is_opaque())
    {
        meta = 3;
    }
    else if (face == +BlockFace::NZ && block_at(world, {pos.x, pos.y, pos.z + 1})->is_opaque())
    {
        meta = 4;
    }
    else
    {
        // Select any face that's available
        if (block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque())
            meta = 1;
        else if (block_at(world, {pos.x + 1, pos.y, pos.z})->is_opaque())
            meta = 2;
        else if (block_at(world, {pos.x, pos.y, pos.z - 1})->is_opaque())
            meta = 3;
        else if (block_at(world, {pos.x, pos.y, pos.z + 1})->is_opaque())
            meta = 4;
    }
    if (pressed)
        meta |= 8;
    world->set_meta_at(pos, meta);
}

void BlockButton::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    if (!can_place(world, pos))
    {
        world->destroy_block(pos, world->get_block_at(pos));
        return;
    }
    uint8_t meta = world->get_meta_at(pos);

    meta &= 7;

    if ((meta == 1 && !block_at(world, {pos.x - 1, pos.y, pos.z})->is_opaque()) ||
        (meta == 2 && !block_at(world, {pos.x + 1, pos.y, pos.z})->is_opaque()) ||
        (meta == 3 && !block_at(world, {pos.x, pos.y, pos.z - 1})->is_opaque()) ||
        (meta == 4 && !block_at(world, {pos.x, pos.y, pos.z + 1})->is_opaque()))
    {
        world->destroy_block(pos, world->get_block_at(pos));
    }
}

void BlockButton::on_click(EntityPhysical *entity, const Vec3i &pos)
{
    on_use(entity, pos);
}

bool BlockButton::provides_power(World *world, const Vec3i &pos, uint8_t face)
{
    return (world->get_meta_at(pos) & 8) != 0;
}

bool BlockButton::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    World *world = entity->world;
    uint8_t meta = world->get_meta_at(pos);

    bool pressed = (meta & 8) != 0;
    meta &= 7;

    if (pressed)
        return true;

    world->set_meta_at(pos, meta | 8);
    Sound button_sound = get_sound("random/click");
    button_sound.volume = 0.3f;
    button_sound.pitch = 0.6f;
    button_sound.position = Vec3f(pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
    world->play_sound(button_sound);
    world->notify_at(pos);
    if (meta == 1)
        world->notify_at({pos.x - 1, pos.y, pos.z});
    else if (meta == 2)
        world->notify_at({pos.x + 1, pos.y, pos.z});
    else if (meta == 3)
        world->notify_at({pos.x, pos.y, pos.z - 1});
    else if (meta == 4)
        world->notify_at({pos.x, pos.y, pos.z + 1});
    world->schedule_block_update(pos, data.block_id, 20);

    return true;
}

void BlockButton::on_removed(World *world, const Vec3i &pos)
{
    uint8_t meta = world->get_meta_at(pos);
    if ((meta & 8) != 0)
    {
        meta &= 7;

        world->notify_at(pos);
        if (meta == 1)
            world->notify_at({pos.x - 1, pos.y, pos.z});
        else if (meta == 2)
            world->notify_at({pos.x + 1, pos.y, pos.z});
        else if (meta == 3)
            world->notify_at({pos.x, pos.y, pos.z - 1});
        else if (meta == 4)
            world->notify_at({pos.x, pos.y, pos.z + 1});
    }
}

bool BlockButton::provides_indirect_power(World *world, const Vec3i &pos, uint8_t face)
{
    uint8_t meta = world->get_meta_at(pos);
    if ((meta & 8) == 0)
        return false;

    return (meta == 1 && face == +BlockFace::NX) ||
           (meta == 2 && face == +BlockFace::PX) ||
           (meta == 3 && face == +BlockFace::NZ) ||
           (meta == 4 && face == +BlockFace::PZ);
}

void BlockButton::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    if (world->is_remote())
        return;
    uint8_t meta = world->get_meta_at(pos);
    if ((meta & 8) != 0)
    {
        world->set_block_and_meta_at(pos, data.block_id, meta & 7);
        world->notify_at(pos);
        if (meta == 1)
            world->notify_at({pos.x - 1, pos.y, pos.z});
        else if (meta == 2)
            world->notify_at({pos.x + 1, pos.y, pos.z});
        else if (meta == 3)
            world->notify_at({pos.x, pos.y, pos.z - 1});
        else if (meta == 4)
            world->notify_at({pos.x, pos.y, pos.z + 1});

        Sound button_sound = get_sound("random/click");
        button_sound.volume = 0.3f;
        button_sound.pitch = 0.5f;
        button_sound.position = Vec3f(pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
        world->play_sound(button_sound);
        world->mark_block_dirty(pos);
    }
}
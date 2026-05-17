#include "block_jukebox.hpp"

#include <world/entity.hpp>
#include <world/world.hpp>
#include <item/item_id.hpp>
#include <item/item_stack.hpp>

BlockJukebox::BlockJukebox(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::WOOD)
{
}

uint8_t BlockJukebox::texture_index(World *world, const Vec3i &pos, uint8_t face)
{
    if (face == +BlockFace::PY)
        return data.texture_index + 1;
    return data.texture_index;
}

bool BlockJukebox::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    World *world = entity->world;
    if (world->get_meta_at(pos) == 0)
    {
        // Playing a disc is handled later
        return false;
    }
    eject(world, pos);
    return true;
}

void BlockJukebox::eject(World *world, const Vec3i &pos)
{
    // TODO: Stop music
    int disc_id = world->get_meta_at(pos);
    disc_id += +ItemID::record_13 - 1;
    world->set_meta_at(pos, 0);
    world->spawn_drop(pos, item::ItemStack(disc_id, 1));
}

void BlockJukebox::drop_item_with_chance(World *world, const Vec3i &pos, uint8_t meta, float chance)
{
    if (meta)
        eject(world, pos);
    BlockBase::drop_item_with_chance(world, pos, meta, chance);
}
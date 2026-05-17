#include "block_container.hpp"
#include <world/world.hpp>

BlockContainer::BlockContainer(uint16_t id, Materials material) : BlockBase(id, material)
{
}

BlockContainer::BlockContainer(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
}

void BlockContainer::on_added(World *world, const Vec3i &pos)
{
    if (world->get_tile_entity(pos))
        return;
    TileEntity *te = get_tile_entity(world, pos);
    if (!te)
        return;
    te->pos = pos;
    te->chunk = world->get_chunk_from_pos(pos);
    te->chunk->tile_entities.push_back(te);
}

void BlockContainer::on_removed(World *world, const Vec3i &pos)
{
    TileEntity *te = world->get_tile_entity(pos);
    if (te)
        te->remove();
}

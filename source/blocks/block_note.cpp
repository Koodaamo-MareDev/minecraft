#include "block_note.hpp"
#include <world/world.hpp>
#include <registry/block_list.hpp>
#include <world/tile_entity/tile_entity_note.hpp>

BlockNote::BlockNote(uint16_t id) : BlockContainer(id, 74, Materials::WOOD)
{
}

void BlockNote::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id)
{
    if (block_list[neighbor_id]->is_power_source())
    {
        bool powered = has_indirect_power(world, pos);

        TileEntityNote *note_entity = dynamic_cast<TileEntityNote *>(world->get_tile_entity(pos));
        if (note_entity)
        {
            if (note_entity->prev_redstone_state != powered)
            {
                if (powered)
                    note_entity->trigger_note(world, pos);
            }
            note_entity->prev_redstone_state = powered;
        }
    }
}

bool BlockNote::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    World *world = entity->world;
    if (world->is_remote())
        return true;
    TileEntityNote *note_entity = dynamic_cast<TileEntityNote *>(world->get_tile_entity(pos));
    if (note_entity)
    {
        note_entity->change_pitch();
        note_entity->trigger_note(world, pos);
    }
    return true;
}

void BlockNote::on_click(EntityPhysical *entity, const Vec3i &pos)
{
    World *world = entity->world;
    if (world->is_remote())
        return;
    TileEntityNote *note_entity = dynamic_cast<TileEntityNote *>(world->get_tile_entity(pos));
    if (note_entity)
    {
        note_entity->trigger_note(world, pos);
    }
}

TileEntity *BlockNote::get_tile_entity(World *world, const Vec3i &pos)
{
    return new TileEntityNote;
}

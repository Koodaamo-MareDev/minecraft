#include "block_furnace.hpp"

#include <world/tile_entity/tile_entity_furnace.hpp>
#include <world/world.hpp>
#include <world/entity.hpp>
#include <registry/block_list.hpp>
#include <gui/gui_smelting.hpp>

BlockFurnace::BlockFurnace(uint16_t id, Materials material) : BlockContainer(id, material)
{
    data.texture_index = 45;
    data.sound_type = BlockSoundType::stone;
}

void BlockFurnace::on_added(World *world, const Vec3i &pos)
{
    BlockContainer::on_added(world, pos);
    set_default_direction(world, pos);
}

void BlockFurnace::on_destroyed(World *world, const Vec3i &pos)
{
    TileEntityFurnace *furnace = dynamic_cast<TileEntityFurnace *>(world->get_tile_entity(pos));
    if (furnace)
        for (size_t i = 0; i < furnace->items.size(); i++)
        {
            item::ItemStack &stack = furnace->items[i];
            if (!stack.empty())
                world->spawn_drop(pos, stack);
        }
}

void BlockFurnace::on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity)
{
    //const uint8_t facing_map[] = {3, 5, 2, 4};
    const uint8_t facing_map[] = {3, 5, 2, 4};
    int facing = int(entity->rotation.y / 90 + 0.5) & 3;

    world->set_meta_at(pos, facing_map[facing]);
}

bool BlockFurnace::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    TileEntity *tile_entity = entity->world->get_tile_entity(pos);

    // Ensure it's a furnace
    TileEntityFurnace *furnace = dynamic_cast<TileEntityFurnace *>(tile_entity);
    if (furnace)
    {
        Gui::set_gui(new GuiSmelting(entity, &furnace->items, 0, furnace));
    }
    return true;
}

TileEntity *BlockFurnace::get_tile_entity(World *world, const Vec3i &pos)
{
    return new TileEntityFurnace;
}

void BlockFurnace::update_state(World *world, const Vec3i &pos, bool state)
{
    world->set_block_and_meta_at(pos, state ? BlockID::lit_furnace : BlockID::furnace, world->get_meta_at(pos));
}

void BlockFurnace::set_default_direction(World *world, const Vec3i &pos)
{
    BlockBase *nx_block = block_at(world, pos + Vec3i{-1, 0, 0});
    BlockBase *px_block = block_at(world, pos + Vec3i{+1, 0, 0});
    BlockBase *nz_block = block_at(world, pos + Vec3i{0, 0, -1});
    BlockBase *pz_block = block_at(world, pos + Vec3i{0, 0, +1});

    uint8_t dir = 3;
    if (px_block->is_opaque() && !nx_block->is_opaque())
        dir = 2;
    if (nz_block->is_opaque() && !pz_block->is_opaque())
        dir = 5;
    if (pz_block->is_opaque() && !nz_block->is_opaque())
        dir = 4;

    world->set_meta_at(pos, dir);
}

uint8_t BlockFurnace::texture_index(World *world, const Vec3i &pos, uint8_t face)
{
    uint8_t meta = world->get_meta_at(pos);
    return face_texture_index(face, meta);
}

uint8_t BlockFurnace::face_texture_index(uint8_t face, uint8_t meta)
{
    if (face == +BlockFace::NY || face == +BlockFace::PY)
        return data.texture_index + 17;
    if (face == +BlockFace::NX && meta == 4)
        return data.texture_index - 1;
    if (face == +BlockFace::PX && meta == 5)
        return data.texture_index - 1;
    if (face == +BlockFace::NZ && meta == 2)
        return data.texture_index - 1;
    if (face == +BlockFace::PZ && meta == 3)
        return data.texture_index - 1;
    return data.block_id == +BlockID::lit_furnace ? data.texture_index + 16 : data.texture_index;
}
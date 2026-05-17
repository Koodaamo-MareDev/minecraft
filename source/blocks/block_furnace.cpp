#include "block_furnace.hpp"

#include <world/tile_entity/tile_entity_furnace.hpp>
#include <world/world.hpp>
#include <world/entity.hpp>
#include <block/block_list.hpp>

BlockFurnace::BlockFurnace(uint16_t id, Materials material) : BlockContainer(id, material)
{
    data.sound_type = BlockSoundType::stone;
}

void BlockFurnace::on_added(World *world, const Vec3i &pos)
{
    BlockContainer::on_added(world, pos);
    set_default_direction(world, pos);
}

void BlockFurnace::on_removed(World *world, const Vec3i &pos)
{
}

void BlockFurnace::on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity)
{
    const uint8_t facing_map[] = {3, 5, 2, 4};
    int facing = int(entity->rotation.y / 90 + 0.5) & 3;

    world->set_meta_at(pos, facing_map[facing]);
}

bool BlockFurnace::on_use(EntityPhysical *entity, const Vec3i &pos)
{
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
    if (face == +BlockFace::NX && meta == 3)
        return data.texture_index;
    if (face == +BlockFace::PX && meta == 2)
        return data.texture_index;
    if (face == +BlockFace::NZ && meta == 5)
        return data.texture_index;
    if (face == +BlockFace::PZ && meta == 4)
        return data.texture_index;
    return data.block_id == +BlockID::lit_furnace ? data.texture_index + 16 : data.texture_index - 1;
}
#include "block_pumpkin.hpp"
#include <world/world.hpp>
#include <registry/block_list.hpp>

BlockPumpkin::BlockPumpkin(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
    data.tick_on_load = true;
}

uint8_t BlockPumpkin::face_texture_index(uint8_t face, uint8_t meta)
{
    uint8_t is_lit = data.block_id == BlockID::lit_pumpkin;
    if (face == +BlockFace::NY || face == +BlockFace::PY)
        return data.texture_index;
    else
    {
        if ((meta == 0 && face == +BlockFace::NZ) ||
            (meta == 1 && face == +BlockFace::PX) ||
            (meta == 2 && face == +BlockFace::PZ) ||
            (meta == 3 && face == +BlockFace::NX))
            return data.texture_index + 17 + is_lit;
        else
            return data.texture_index + 16;
    }
}

bool BlockPumpkin::can_place(World *world, const Vec3i &pos)
{
    BlockID old = world->get_block_id_at(pos);
    return (old == BlockID::air || block_list[old]->material().is_liquid) || block_at(world, pos + Vec3i{0, -1, 0})->is_opaque();
}

void BlockPumpkin::on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity)
{
    int facing = int(entity->rotation.y / 90 + 0.5) & 3;
    const uint8_t facing_map[] = {3, 5, 2, 4};
    world->set_meta_at(pos, facing_map[facing]);
}
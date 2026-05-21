#include "block_fluids.hpp"

#include <render/render_blocks.hpp>
#include <world/world.hpp>
#include <world/chunk.hpp>
#include <registry/block_list.hpp>

BlockFluids::BlockFluids(uint16_t id, Materials material) : BlockBase(id, (material == Materials::LAVA ? 14 : 12) * 16 + 13, material)
{
    data.render_func = render_nothing;
    data.liquid = true;
    data.tick_on_load = true;
}

bool BlockFluids::is_opaque()
{
    return false;
}

uint8_t BlockFluids::texture_index(World *world, const Vec3i &pos, uint8_t face)
{
    if (face == +BlockFace::NY || face == +BlockFace::PY)
        return data.texture_index;
    return data.texture_index + 1;
}

bool BlockFluids::should_render_side(World *world, const Vec3i &pos, uint8_t face)
{
    Materials material = block_at(world, pos)->material_type();
    if (material == data.material || material == Materials::ICE)
        return false;
    if (face == +BlockFace::PY)
        return true;
    return BlockBase::should_render_side(world, pos, face);
}

void BlockFluids::apply_entity_velocity(EntityPhysical *entity, const Vec3i &pos)
{
    // TODO
}

void BlockFluids::on_added(World *world, const Vec3i &pos)
{
    harden(world, pos);
}

void BlockFluids::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
}

void BlockFluids::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    harden(world, pos);
}

int BlockFluids::tick_rate()
{
    return data.material == Materials::WATER ? 5 : 30;
}

int BlockFluids::get_fluid_level_at(World *world, const Vec3i &pos)
{
    BlockBase *block = block_at(world, pos);
    if (block->material_type() == data.material)
        return world->get_meta_at(pos);

    return -1;
}

int BlockFluids::get_min_fluid_level(World *world, const Vec3i &pos, int src_level, int &sources)
{
    int fluid_level = get_fluid_level_at(world, pos);

    // If the fluid level is invalid, default to the original fluid level
    if (fluid_level < 0)
        return src_level;

    // If the fluid level is a source block, return 0
    if (fluid_level == 0)
        sources++;

    if (fluid_level >= 8)
        fluid_level = 0;

    if (src_level >= 0 && fluid_level >= src_level)
        return src_level;

    return fluid_level;
}

void BlockFluids::set_fluid_stationary(World *world, const Vec3i &pos)
{
    // Set the block to the base fluid, keeping the same meta
    world->set_block_and_meta_at(pos, BlockID::water, world->get_meta_at(pos));
}

void BlockFluids::harden(World *world, const Vec3i &pos)
{
    BlockBase *block = block_at(world, pos);
    if (block->material_type() != Materials::LAVA)
        return;

    for (int direction = 0; direction < 6; direction++)
    {
        if (direction == FACE_NY)
            continue;
        Vec3i offset = face_offsets[direction];
        if (block_at(world, pos + offset)->material_type() == Materials::WATER)
        {
            int fluid_level = world->get_meta_at(pos);
            if (fluid_level == 0)
            {
                world->set_block_at(pos, BlockID::obsidian);
                world->notify_at(pos);
                return;
            }
            else if (fluid_level <= 4)
            {
                world->set_block_at(pos, BlockID::cobblestone);
                world->notify_at(pos);
                return;
            }
        }
    }
}

float BlockFluids::get_height(int level)
{
    if (level >= 8)
        return 1 / 9.0f;
    return (level + 1) / 9.0f;
}

bool BlockFluids::can_any_fluid_replace(BlockID id)
{
    switch (id)
    {
    case BlockID::air:
        return true;
    case BlockID::wooden_door:
    case BlockID::iron_door:
    case BlockID::standing_sign:
    case BlockID::ladder:
    case BlockID::reeds:
        return false;
    default:
        return !properties(id).m_solid;
    }
}

bool BlockFluids::can_fluid_replace(BlockID fluid, BlockID id)
{
    BlockProperties &fluid_props = properties(fluid);

    // Sanity check
    if (!fluid_props.m_fluid)
        return false;

    BlockProperties &props = properties(id);
    if (fluid_props.m_base_fluid == props.m_base_fluid)
        return false;

    if (props.m_base_fluid == BlockID::lava)
        return false;

    return can_any_fluid_replace(id);
}

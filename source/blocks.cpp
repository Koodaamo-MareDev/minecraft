#include "blocks.hpp"
#include "blockmap_alpha.h"
#include "vec3i.hpp"
#include "chunk_new.hpp"
#include "light.hpp"
#include <cmath>
int8_t get_block_opacity(BlockID blockid)
{
    switch (blockid)
    {
    case BlockID::air:
    case BlockID::glass:
    case BlockID::leaves:
    case BlockID::glowstone:
        return 0;
    case BlockID::water:
    case BlockID::flowing_water:
        return 1;
    default:
        return 15;
    }
}

bool is_face_transparent(uint8_t texture_index)
{
    return blockmap_alpha[texture_index] == 0;
}

bool is_solid(BlockID block_id)
{
    return block_id != BlockID::air && !is_fluid(block_id) && !is_face_transparent(get_default_texture_index(block_id));
}

uint8_t get_block_luminance(BlockID block_id)
{
    switch (block_id)
    {
    case BlockID::lava:
    case BlockID::flowing_lava:
    case BlockID::glowstone:
        return 15;

    default:
        return 0;
    }
}

uint32_t get_default_texture_index(BlockID blockid)
{
    switch (blockid)
    {
    case BlockID::stone:
        return 0;
    case BlockID::grass:
        return 3;
    case BlockID::dirt:
        return 2;
    case BlockID::cobblestone:
        return 16;
    case BlockID::planks:
        return 4;
    case BlockID::sapling:
        return 15;
    case BlockID::bedrock:
        return 17;
    case BlockID::flowing_water:
    case BlockID::water:
        return 205;
    case BlockID::flowing_lava:
    case BlockID::lava:
        return 237;
    case BlockID::sand:
        return 18;
    case BlockID::gravel:
        return 19;
    case BlockID::gold_ore:
        return 32;
    case BlockID::iron_ore:
        return 33;
    case BlockID::coal_ore:
        return 34;
    case BlockID::wood:
        return 20;
    case BlockID::leaves:
        return 52;
    case BlockID::sponge:
        return 48;
    case BlockID::glass:
        return 49;
    case BlockID::lapis_ore:
        return 160;
    case BlockID::lapis_block:
        return 144;
    case BlockID::dispenser:
        return 45;
    case BlockID::sandstone:
        return 176;
    case BlockID::note_block:
        return 74;
    case BlockID::bed: // TODO
        return 135;
    case BlockID::golden_rail:
        return 163;
    case BlockID::detector_rail:
        return 179;
    case BlockID::netherrack:
        return 103;
    case BlockID::soul_sand:
        return 104;
    case BlockID::glowstone:
        return 105;
    default:
        return int(blockid) & 0xFF;
    }
}

uint32_t get_face_texture_index(block_t *block, int face)
{
    if (!block)
        return 0;
    const int directionmap[] = {
        FACE_NX,
        FACE_PX,
        FACE_NZ,
        FACE_PZ};
    BlockID blockid = block->get_blockid();
    // Have to handle differently for different blocks, e.g. grass, logs, crafting table
    switch (blockid)
    {
    case BlockID::stone:
        return 0;
    case BlockID::grass:
    {
        switch (face)
        {
        case FACE_NX:
        case FACE_PX:
        case FACE_NZ:
        case FACE_PZ:
            return 3;
        case FACE_NY:
            return 2;
        case FACE_PY:
            return 1;
        }
    }
    case BlockID::wood:
    { // log
        switch (face)
        {
        case FACE_NY:
        case FACE_PY:
            return 21;
        default:
            return 20;
        }
    }
    case BlockID::dispenser:
    {
        int block_direction = (block->meta) & 0x03;
        if (face == FACE_NY || face == FACE_PY)
            return 62;
        if (face == directionmap[block_direction])
            return 46;
        return 45;
    }
    default:
        return get_default_texture_index(blockid);
    }
}

void update_fluid(block_t *block, vec3i pos)
{
    if (!(block->meta & FLUID_UPDATE_REQUIRED_FLAG))
        return;
    block->meta &= ~FLUID_UPDATE_REQUIRED_FLAG;
    BlockID block_id = block->get_blockid();
    if (!is_fluid(block_id))
        return;

    uint8_t old_level = get_fluid_meta_level(block);
    uint8_t level = 0;
    // All directions except +Y because of gravity.
    block_t *nx = get_block_at(pos + face_offsets[FACE_NX]);
    block_t *px = get_block_at(pos + face_offsets[FACE_PX]);
    block_t *ny = get_block_at(pos + face_offsets[FACE_NY]);
    block_t *py = get_block_at(pos + face_offsets[FACE_PY]);
    block_t *nz = get_block_at(pos + face_offsets[FACE_NZ]);
    block_t *pz = get_block_at(pos + face_offsets[FACE_PZ]);
    block_t *surroundings[6] = {ny, nx, px, nz, pz, py};
    int surrounding_dirs[6] = {FACE_NY, FACE_NX, FACE_PX, FACE_NZ, FACE_PZ, FACE_PY};
    if (is_flowing_fluid(block_id))
    {
        uint8_t surrounding_sources = 0;
        uint8_t min_surrounding_level = 7;
        for (int i = 1; i < 5; i++) // Skip negative y
        {
            block_t *surrounding = surroundings[i];
            if (surrounding)
            {
                BlockID surround_id = surrounding->get_blockid();
                if (is_still_fluid(surround_id))
                {
                    surrounding_sources++;
                }
                if (is_same_fluid(surround_id, block_id))
                {
                    min_surrounding_level = std::min(get_fluid_meta_level(surrounding), min_surrounding_level);
                }
            }
        }
        BlockID above_id = py ? py->get_blockid() : BlockID::air;
        if (is_same_fluid(block_id, above_id))
            min_surrounding_level = 0;
        level = min_surrounding_level + 1;

        if (surrounding_sources >= 2)
        {
            BlockID ny_blockid = ny ? ny->get_blockid() : BlockID::air;
            // Convert to source only if the fluid cannot flow downwards.
            if ((!is_fluid_overridable(ny_blockid) && !is_fluid(ny_blockid)))
            {
                block->set_blockid(basefluid(block_id));
                block->meta &= ~0xF;
            }
        }
        // Level 8 -> air
        else if (level >= 8)
        {
            block->set_blockid(BlockID::air);
            block->meta &= ~0xF;
        }
        else
            set_fluid_level(block, level);
    }
    level = get_fluid_meta_level(block);
    //  Fluid spread:
    if (level < 7)
    {
        for (int i = 0; i < 5; i++)
        {
            block_t *surrounding = surroundings[i];
            if (surrounding)
            {
                BlockID surround_id = surrounding->get_blockid();
                vec3i surrounding_offset = face_offsets[surrounding_dirs[i]];
                if (is_fluid_overridable(surround_id))
                {
                    int surrounding_level = get_fluid_meta_level(surrounding);
                    if (surrounding_dirs[i] == FACE_NY)
                    {
                        if (surround_id != flowfluid(block_id) || surrounding_level != 0)
                        {
                            surrounding->meta |= FLUID_UPDATE_REQUIRED_FLAG; // FLUID_UPDATE_REQUIRED_FLAG * (2 - (std::signbit(surrounding_offset.x) | std::signbit(surrounding_offset.z)));
                            surrounding->set_blockid(flowfluid(block_id));
                            set_fluid_level(surrounding, 0);
                            update_light(pos + surrounding_offset);
                        }
                        break;
                    }
                    else
                    {
                        if (surround_id != flowfluid(block_id) || surrounding_level > level + 1)
                        {
                            surrounding->meta |= FLUID_UPDATE_REQUIRED_FLAG; // FLUID_UPDATE_REQUIRED_FLAG * (2 - (std::signbit(surrounding_offset.x) | std::signbit(surrounding_offset.z)));
                            surrounding->set_blockid(flowfluid(block_id));
                            set_fluid_level(surrounding, level + 1);
                            update_light(pos + surrounding_offset);
                        }
                    }
                }
            }
        }
    }
    bool changed = false;
    if (block->get_blockid() != block_id)
    {
        changed = true;
        update_light(pos);
    }
    if (level != old_level && is_flowing_fluid(block->get_blockid()))
    {
        changed = true;
        set_fluid_level(block, level);
    }
    if (changed)
    {
        for (int i = 0; i < 5; i++)
        {
            vec3i offset_pos = pos + face_offsets[surrounding_dirs[i]];
            chunk_t *chunk = get_chunk_from_pos(offset_pos, false, false);
            if (chunk)
                chunk->vbos[pos.y / 16].dirty = true;
            block_t *other = chunk->get_block(offset_pos);
            if (other)
                other->meta |= FLUID_UPDATE_REQUIRED_FLAG;
        }
    }
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 1; z++)
            {
                vec3i offset_pos = pos + vec3i(x, y, z);
                chunk_t *chunk = get_chunk_from_pos(offset_pos, false, false);
                block_t *other = chunk->get_block(offset_pos);
                if (other)
                    update_light(offset_pos);
            }
}

void set_fluid_level(block_t *block, uint8_t level)
{
    if (block)
        block->meta = (block->meta & ~0xF) | level;
}

// Blatantly stolen from the notchian client.
// All I did was make it human readable and
// converted it into suitable C++ code.
// - Marcus
float get_percent_air(int fluid_level)
{
    if (fluid_level >= 8)
    {
        fluid_level = 0;
    }
    return (float)(fluid_level + 1) / 9.0F;
}

// Blatantly stolen from the notchian client.
// All I did was make it human readable and
// converted it into suitable C++ code.
// - Marcus
float get_fluid_height(vec3i pos, BlockID block_type)
{
    int block_x = pos.x, block_y = pos.y, block_z = pos.z;
    int surrounding_water = 0;
    float water_percentage = 0.0F;

    for (int i = 0; i < 4; ++i)
    {
        int check_x = block_x - (i & 1);
        int check_z = block_z - (i >> 1 & 1);
        if (is_same_fluid(get_block_id_at(vec3i(check_x, block_y + 1, check_z)), block_type))
        {
            return 1.0F;
        }

        BlockID check_block_type = get_block_id_at(vec3i(check_x, block_y, check_z));
        if (!is_same_fluid(check_block_type, block_type))
        {
            if (!is_solid(check_block_type))
            {
                ++water_percentage;
                ++surrounding_water;
            }
        }
        else
        {
            int fluid_level = get_fluid_meta_level(get_block_at(vec3i(check_x, block_y, check_z)));
            if (fluid_level >= 8 || fluid_level == 0)
            {
                water_percentage += get_percent_air(fluid_level) * 10.0F;
                surrounding_water += 10;
            }

            water_percentage += get_percent_air(fluid_level);
            ++surrounding_water;
        }
    }

    return 1.0F - water_percentage / (float)surrounding_water;
}

uint8_t get_fluid_meta_level(block_t *block)
{
    if (block)
    {
        BlockID block_id = block->get_blockid();
        if (is_still_fluid(block_id))
            return 0;
        if (is_flowing_fluid(block_id))
            return block->meta & 0xF;
    }
    return 8;
}

uint8_t get_fluid_visual_level(vec3i pos, BlockID block_id)
{
    return FLOAT_TO_FLUIDMETA(get_fluid_height(pos, block_id));
}
bool is_fluid_overridable(BlockID id)
{
    switch (id)
    {
    case BlockID::air:
    case BlockID::flowing_water:
    case BlockID::flowing_lava:
        return true;

    default:
        return false;
    }
}
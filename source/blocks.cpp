#include "block.hpp"
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
    case BlockID::glowstone:
        return 0;
    case BlockID::leaves:
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
    case BlockID::torch:
        return 14;

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
        return 206;
    case BlockID::water:
        return 205;
    case BlockID::flowing_lava:
        return 238;
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
    case BlockID::torch:
        return 80;
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

void update_fluid(block_t *block, vec3i pos, chunk_t *near)
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
    block_t *nx = get_block_at(pos + face_offsets[FACE_NX], near);
    block_t *px = get_block_at(pos + face_offsets[FACE_PX], near);
    block_t *ny = get_block_at(pos + face_offsets[FACE_NY], near);
    block_t *py = get_block_at(pos + face_offsets[FACE_PY], near);
    block_t *nz = get_block_at(pos + face_offsets[FACE_NZ], near);
    block_t *pz = get_block_at(pos + face_offsets[FACE_PZ], near);
    block_t *surroundings[6] = {ny, nx, px, nz, pz, py};
    int surrounding_dirs[6] = {FACE_NY, FACE_NX, FACE_PX, FACE_NZ, FACE_PZ, FACE_PY};

    for (int i = 0; i < 6; i++)
    {
        block_t *surrounding = surroundings[i];
        if (surrounding)
        {
            BlockID surrounding_id = surrounding->get_blockid();
            if (is_fluid(surrounding_id) && !is_same_fluid(surrounding_id, block_id))
            {
                bool surrounding_changed = false;
                if (surrounding_dirs[i] != FACE_PY && basefluid(block_id) == BlockID::water)
                {
                    if (surrounding_id == BlockID::lava)
                        surrounding->set_blockid(BlockID::obsidian);
                    else
                        surrounding->set_blockid(BlockID::cobblestone);
                    surrounding->meta = 0;
                    update_light(pos + face_offsets[surrounding_dirs[i]]);

                    surrounding_changed = true;
                }
                else if (surrounding_dirs[i] == FACE_NY && basefluid(block_id) == BlockID::lava)
                {
                    surrounding->set_blockid(BlockID::stone);
                    surrounding->meta = 0;
                    update_light(pos + face_offsets[FACE_NY]);

                    surrounding_changed = true;
                }
                else if (basefluid(block_id) == BlockID::lava)
                {
                    block->set_blockid(BlockID::cobblestone);
                    block->meta = 0;
                    update_light(pos);
                    
                    for (int j = 0; j < 6; j++)
                    {
                        block_t *other = surroundings[j];
                        if (other && is_fluid(other->get_blockid()))
                        {
                            update_light(pos + face_offsets[j]);
                            other->meta |= FLUID_UPDATE_REQUIRED_FLAG;
                        }
                    }
                    return;
                }
                if (surrounding_changed)
                {
                    block_t *neighbors[6];
                    get_neighbors(pos + face_offsets[surrounding_dirs[i]], neighbors);
                    for (int j = 0; j < 6; j++)
                    {
                        block_t *other = neighbors[j];
                        if (other && is_fluid(other->get_blockid()))
                        {
                            update_light(pos + face_offsets[j]);
                            other->meta |= FLUID_UPDATE_REQUIRED_FLAG;
                        }
                    }
                    return;
                }
            }
        }
    }

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
    if (level <= 7 && pos.y > 0)
    {
        int flow_weights[4] = {1000, 1000, 1000, 1000};
        int min_weight = 1000;
        // Calculate horizontal flow weights
        for (int i = 0; i < 4; i++)
        {
            vec3i offset = face_offsets[surrounding_dirs[i + 1]];
            for (int j = 1; j <= 5; j++)
            {
                vec3i other_pos = pos + (j * offset);
                BlockID other_id = get_block_id_at(other_pos, BlockID::stone, near);
                if (other_id != BlockID::air)
                    break;

                other_pos = other_pos + vec3i(0, -1, 0);
                other_id = get_block_id_at(other_pos, BlockID::air, near);
                if (is_same_fluid(other_id, block_id) || is_fluid_overridable(other_id))
                {
                    flow_weights[i] = j;
                    break;
                }
            }
            min_weight = std::min(min_weight, flow_weights[i]);
        }

        for (int i = 0; i < 5; i++)
        {
            if (i != 0 && flow_weights[i - 1] != min_weight)
                continue;
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
                    else if (level < 7)
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
                else if (surrounding_dirs[i] == FACE_NY && is_same_fluid(surround_id, block_id))
                {
                    break;
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
            {
                other->meta |= FLUID_UPDATE_REQUIRED_FLAG;
                update_light(offset_pos);
            }
        }
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
float get_fluid_height(vec3i pos, BlockID block_type, chunk_t *near)
{
    int block_x = pos.x, block_y = pos.y, block_z = pos.z;
    int surrounding_water = 0;
    float water_percentage = 0.0F;

    for (int i = 0; i < 4; ++i)
    {
        int check_x = block_x - (i & 1);
        int check_z = block_z - (i >> 1 & 1);
        if (is_same_fluid(get_block_id_at(vec3i(check_x, block_y + 1, check_z), BlockID::air, near), block_type))
        {
            return 1.0F;
        }

        BlockID check_block_type = get_block_id_at(vec3i(check_x, block_y, check_z), BlockID::air, near);
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
            int fluid_level = get_fluid_meta_level(get_block_at(vec3i(check_x, block_y, check_z), near));
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

uint8_t get_fluid_visual_level(vec3i pos, BlockID block_id, chunk_t *near)
{
    return FLOAT_TO_FLUIDMETA(get_fluid_height(pos, block_id, near));
}
bool is_fluid_overridable(BlockID id)
{
    switch (id)
    {
    case BlockID::air:
    case BlockID::flowing_water:
    case BlockID::flowing_lava:
    case BlockID::torch:
        return true;

    default:
        return false;
    }
}

blockproperties_t block_properties[256] = {
    {BlockID::air, 0, 0, 0, 1, 0, 0, 0, 0, BlockID::air, BlockID::air},
    {BlockID::stone, 0, 0, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::grass, 0, 3, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::dirt, 0, 2, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::cobblestone, 0, 16, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::planks, 0, 4, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::sapling, 0, 15, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::bedrock, 0, 17, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::flowing_water, 0, 205, 15, 0, 0, 1, 1, 1, BlockID::water, BlockID::flowing_water},
    {BlockID::water, 0, 205, 15, 0, 0, 1, 1, 1, BlockID::water, BlockID::flowing_water},
    {BlockID::flowing_lava, 0, 237, 15, 0, 15, 1, 1, 2, BlockID::lava, BlockID::flowing_lava},
    {BlockID::lava, 0, 237, 15, 0, 15, 1, 1, 2, BlockID::lava, BlockID::flowing_lava},
    {BlockID::sand, 0, 18, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::gravel, 0, 19, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::gold_ore, 0, 32, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::iron_ore, 0, 33, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::coal_ore, 0, 34, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::wood, 0, 20, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::leaves, 0, 52, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::sponge, 0, 48, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::glass, 0, 49, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::lapis_ore, 0, 160, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::lapis_block, 0, 144, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::dispenser, 0, 45, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::sandstone, 0, 176, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::note_block, 0, 74, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::bed, 0, 135, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::golden_rail, 0, 163, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::detector_rail, 0, 179, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::sticky_piston, 0, 29, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::cobweb, 0, 11, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::tallgrass, 0, 38, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::deadbush, 0, 54, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::piston, 0, 108, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::piston_head, 0, 107, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::wool, 0, 64, 15, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::air, 0, 0, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air}, // 35 - unused
    {BlockID::dandelion, 0, 13, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::rose, 0, 12, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
    {BlockID::brown_mushroom, 0, 29, 0, 0, 0, 1, 0, 0, BlockID::air, BlockID::air},
};
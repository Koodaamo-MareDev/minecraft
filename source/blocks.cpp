#include "blocks.hpp"
#include "blockmap_alpha.h"
#include "vec3i.hpp"
#include "chunk_new.hpp"
#include "light.hpp"
#include <cmath>
int get_block_opacity(BlockID blockid)
{
    switch (blockid)
    {
    case BlockID::air:
    case BlockID::glass:
    case BlockID::leaves:
        return 0;
    case BlockID::water:
    case BlockID::flowing_water:
        return 1;
    default:
        return 15;
    }
}

bool is_solid(BlockID block_id)
{
    return block_id != BlockID::air && !is_fluid(block_id);
}

bool is_face_transparent(int texture_index)
{
    return blockmap_alpha[texture_index & 0xFF] == 0;
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
    int default_tex_ind = block->id & 0xFF;
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
        return 239;
    case BlockID::lava:
        return 239;
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
    {
        int block_direction = (block->meta) & 0x03;
        if (face == FACE_NY || face == FACE_PY)
            return 62;
        if (face == directionmap[block_direction])
            return 46;
        return 45;
    }
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
    default:
        return default_tex_ind;
    }
}

void update_fluid(block_t *block, vec3i pos)
{
    BlockID block_id = block->get_blockid();
    if (!(block->meta & FLUID_UPDATE_REQUIRED_FLAG))
        return;

    block->meta &= ~FLUID_UPDATE_REQUIRED_FLAG;
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
        for (int i = 1; i < 6; i++) // Skip negative y
        {
            block_t *surrounding = surroundings[i];
            if (surrounding)
            {
                BlockID surround_id = surrounding->get_blockid();
                if (surrounding_dirs[i] != FACE_PY && is_still_fluid(surround_id))
                {
                    surrounding_sources++;
                }
                if (is_same_fluid(surround_id, block_id))
                {
                    min_surrounding_level = std::min(get_fluid_meta_level(surrounding), min_surrounding_level);
                }
            }
        }
        level = min_surrounding_level + 1;

        if (surrounding_sources >= 2)
        {
            BlockID ny_blockid = ny ? ny->get_blockid() : BlockID::air;
            // Convert to source only if the fluid cannot flow downwards.
            if ((!is_fluid_overridable(ny_blockid) && !is_fluid(ny_blockid)))
            {
                block->set_blockid(BlockID::water);
                block->meta = 0;
            }
        }
        // Level 8 -> air
        else if (level >= 8)
        {
            block->set_blockid(BlockID::air);
            block->meta = 0;
        }
    }
    level = get_fluid_meta_level(block);
    // Fluid spread:
    if (level < 7)
    {
        for (int i = 0; i < 5; i++)
        {
            block_t *surrounding = surroundings[i];
            if (surrounding)
            {
                BlockID surround_id = surrounding->get_blockid();
                vec3i surrounding_pos = pos + face_offsets[surrounding_dirs[i]];
                if (is_fluid_overridable(surround_id))
                {
                    int surrounding_level = get_fluid_meta_level(surrounding);
                    if (surrounding_dirs[i] == FACE_NY)
                    {
                        if (surround_id != flowfluid(block_id) || surrounding_level != 0)
                        {
                            surrounding->meta |= FLUID_UPDATE_REQUIRED_FLAG;
                            update_light(lightupdate_t(surrounding_pos, surrounding->get_blocklight(), surrounding->get_skylight()));
                        }
                        surrounding->set_blockid(flowfluid(surround_id));
                        set_fluid_level(surrounding, 0);
                        break;
                    }
                    else
                    {
                        if (surround_id != flowfluid(block_id) || surrounding_level > level + 1)
                        {
                            surrounding->meta |= FLUID_UPDATE_REQUIRED_FLAG;
                            update_light(lightupdate_t(surrounding_pos, surrounding->get_blocklight(), surrounding->get_skylight()));
                            // get_chunk_from_pos(surrounding_pos.x, surrounding_pos.z, false)->vbos[surrounding_pos.y / 16].dirty = surrounding->get_blockid() != flowfluid(surround_id);
                        }
                        surrounding->set_blockid(flowfluid(surround_id));
                        set_fluid_level(surrounding, level + 1);
                    }
                }
            }
        }
    }
    if (level != old_level || block->get_blockid() != block_id)
        update_light(lightupdate_t(pos, block->get_blocklight(), block->get_skylight()));
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
        if (is_same_fluid(get_block_id_at({check_x, block_y + 1, check_z}), block_type))
        {
            return 1.0F;
        }

        BlockID check_block_type = get_block_id_at({check_x, block_y, check_z});
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
            int fluid_level = get_fluid_meta_level(get_block_at({check_x, block_y, check_z}));
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
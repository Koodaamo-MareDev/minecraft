#include "blocks.hpp"
#include "block.hpp"
#include "blockmap_alpha.h"
#include "vec3i.hpp"
#include "chunk_new.hpp"
#include "light.hpp"
#include <cmath>
int8_t get_block_opacity(BlockID blockid)
{
    return block_properties[int(blockid)].m_opacity;
}

bool is_face_transparent(uint8_t texture_index)
{
    return blockmap_alpha[texture_index] == 0;
}

bool is_solid(BlockID block_id)
{
    return block_properties[uint8_t(block_id)].m_solid;
}

uint8_t get_block_luminance(BlockID block_id)
{
    return block_properties[uint8_t(block_id)].m_luminance;
}

uint32_t get_default_texture_index(BlockID blockid)
{
    return block_properties[uint8_t(blockid)].m_texture_index;
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
            return 202;
        case FACE_NY:
            return 2;
        case FACE_PY:
            return 203;
        }
    }
    case BlockID::leaves:
    {
        if (!block->meta)
            return 186;
        else if (block->meta == 1)
            return 235;
        return 219;
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
    case BlockID::tnt:
    {
        if (face == FACE_NY)
            return 10;
        if (face == FACE_PY)
            return 9;
        return 8;
    }
    case BlockID::bookshelf:
    {
        if (face == FACE_NY || face == FACE_PY)
            return 4;
        return 35;
    }
    case BlockID::chest:
    {
        int block_direction = (block->meta) & 0x03;
        if (face == FACE_NY || face == FACE_PY)
            return 25;
        if (face == directionmap[block_direction])
            return 27;
        return 26;
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
    case BlockID::double_stone_slab:
    case BlockID::stone_slab:
    {
        if (face == FACE_NY || face == FACE_PY)
            return 6;
        return 5;
    }
    case BlockID::crafting_table:
    {
        if (face == FACE_NY)
            return 4;
        if (face == FACE_PY)
            return 43;
        if (face == FACE_NX || face == FACE_PX)
            return 59;
        return 60;
    }
    case BlockID::piston:
    case BlockID::sticky_piston:
    {
        int block_direction = (block->meta) & 0x07;
        bool sticky = (block->meta & 0x08) != 0;
        if (block_direction == face)
            return sticky ? 107 : 106;
        else if (block_direction == (face ^ 1))
            return 109;
        else if ((block_direction & ~1) == FACE_NY)
            return 172 + (block_direction & 1);
        else
            return 188 + ((face & 1) ^ ((face & ~1) == FACE_NY));
    }
    default:
        return get_default_texture_index(blockid);
    }
}

// Returns the BlockID of the fluid that the target fluid will
// convert to when target fluid attempts to override the original fluid.
BlockID fluid_collision_result(BlockID original, BlockID target)
{
    if (is_fluid_overridable(original))
        return target;
    if (!is_fluid(original))
        return original;
    if (is_same_fluid(original, target))
        return original;
    if (is_flowing_fluid(original))
        return BlockID::cobblestone;
    if (original == BlockID::lava)
        return BlockID::obsidian;
    if (original == BlockID::water)
        return BlockID::stone;
    return BlockID::air;
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

    uint8_t max_fluid_level = flowfluid(block_id) == BlockID::flowing_water ? 7 : 3;

    if (is_flowing_fluid(block_id))
    {
        uint8_t surrounding_sources = 0;
        uint8_t min_surrounding_level = max_fluid_level;
        for (int i = 1; i < 5; i++) // Skip negative y
        {
            block_t *surrounding = surroundings[i];
            if (surrounding)
            {
                BlockID surround_id = surrounding->get_blockid();
                if (is_same_fluid(surround_id, block_id))
                {
                    if (is_still_fluid(surround_id))
                    {
                        if (surround_id == BlockID::water)
                            surrounding_sources++;
                    }
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
                update_block_at(pos);
            }
        }
        // Max fluid level + 1 -> air
        else if (level >= max_fluid_level + 1)
        {
            block->set_blockid(BlockID::air);
            block->meta &= ~0xF;
            update_block_at(pos);
        }
        else
            set_fluid_level(block, level);
    }
    level = get_fluid_meta_level(block);

    BlockID flow_id = flowfluid(block_id);
    max_fluid_level = flow_id == BlockID::flowing_water ? 7 : 3;
    //  Fluid spread:
    if (level <= max_fluid_level && pos.y > 0)
    {
        int flow_weights[4] = {1000, 1000, 1000, 1000};
        int min_weight = 1000;
        // Calculate horizontal flow weights
        for (int i = 0; i < 4; i++)
        {
            vec3i offset = face_offsets[surrounding_dirs[i + 1]];
            for (int j = 1; j <= max_fluid_level * 3 / 4; j++)
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
            if (i != 0)
            {
                // Skip horizontal flow if the level is maxed out.
                // Skip the direction if the flow weight is not the minimum.
                if (level >= max_fluid_level || flow_weights[i - 1] != min_weight)
                    continue;
            }
            block_t *surround = surroundings[i];
            if (surround)
            {
                BlockID surround_id = surround->get_blockid();
                vec3i surrounding_offset = face_offsets[surrounding_dirs[i]];
                uint8_t surround_level = get_fluid_meta_level(surround);

                if (is_same_fluid(surround_id, block_id))
                {
                    // Restrict flow horizontally.
                    if (i != 0 && surround_level <= level + 1)
                        continue;

                    // If flowing horizontally, the surrounding fluid level will be current level + 1.
                    // If flowing downwards, the surrounding fluid level will be 0.
                    set_fluid_level(surround, (level + 1) * (i != 0));
                    surround->meta |= FLUID_UPDATE_REQUIRED_FLAG;
                    update_block_at(pos + surrounding_offset);
                    if (i == 0 && is_flowing_fluid(block_id))
                        break;
                }
                else
                {
                    BlockID new_surround_id = fluid_collision_result(surround_id, flow_id);

                    if (new_surround_id != surround_id)
                    {
                        if (is_fluid(new_surround_id))
                        {
                            // Restrict flow horizontally.
                            if (i != 0 && surround_level <= level + 1)
                                continue;

                            // If flowing horizontally, the surrounding fluid level will be current level + 1.
                            // If flowing downwards, the surrounding fluid level will be 0.
                            set_fluid_level(surround, (level + 1) * (i != 0));
                            surround->set_blockid(new_surround_id);
                            update_block_at(pos + surrounding_offset);
                            if (i == 0 && is_flowing_fluid(block_id))
                                break;
                        }
                        else
                        {
                            // Reset meta if the result is not a fluid.
                            surround->meta = 0;
                            surround->set_blockid(new_surround_id);
                            update_block_at(pos + surrounding_offset);
                            update_neighbors(pos + surrounding_offset);
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
        update_block_at(pos);
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
            update_block_at(offset_pos);
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
    case BlockID::torch:
        return true;

    default:
        return false;
    }
}

void default_aabb(const vec3i &pos, block_t *block, const aabb_t &other, std::vector<aabb_t> &aabb_list)
{
    aabb_t aabb;
    aabb.min = vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + vec3f(1, 1, 1);
    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void slab_aabb(const vec3i &pos, block_t *block, const aabb_t &other, std::vector<aabb_t> &aabb_list)
{
    aabb_t aabb;

    aabb.min = vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + vec3f(1, 1, 1);

    if ((block->meta & 1))
    {
        aabb.min.y += 0.5;
    }
    else if (!(block->meta & 2))
    {
        aabb.max.y -= 0.5;
    }

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

blockproperties_t block_properties[256] = {
    blockproperties_t().id(BlockID::air).opacity(0).solid(false).transparent(true).collision(CollisionType::none).valid_item(false),
    blockproperties_t().id(BlockID::stone).texture(0).sound(SoundType::stone),
    blockproperties_t().id(BlockID::grass).texture(1).sound(SoundType::grass).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::dirt).texture(2).sound(SoundType::dirt),
    blockproperties_t().id(BlockID::cobblestone).texture(16).sound(SoundType::stone),
    blockproperties_t().id(BlockID::planks).texture(4).sound(SoundType::wood),
    blockproperties_t().id(BlockID::sapling).texture(15).sound(SoundType::grass).opacity(0).solid(false).transparent(true).render_type(RenderType::cross).collision(CollisionType::none),
    blockproperties_t().id(BlockID::bedrock).texture(17).sound(SoundType::stone).blast_resistance(-1),
    blockproperties_t().id(BlockID::flowing_water).texture(206).fluid(true).base_fluid(BlockID::water).flow_fluid(BlockID::flowing_water).opacity(1).solid(false).transparent(true).render_type(RenderType::special).collision(CollisionType::fluid).blast_resistance(500),
    blockproperties_t().id(BlockID::water).texture(205).fluid(true).base_fluid(BlockID::water).flow_fluid(BlockID::flowing_water).opacity(1).solid(false).transparent(true).render_type(RenderType::special).collision(CollisionType::fluid).blast_resistance(500),
    blockproperties_t().id(BlockID::flowing_lava).texture(238).fluid(true).base_fluid(BlockID::lava).flow_fluid(BlockID::flowing_lava).opacity(0).luminance(15).solid(false).render_type(RenderType::special).collision(CollisionType::fluid).blast_resistance(500),
    blockproperties_t().id(BlockID::lava).texture(237).fluid(true).base_fluid(BlockID::lava).flow_fluid(BlockID::flowing_lava).opacity(0).luminance(15).solid(false).render_type(RenderType::special).collision(CollisionType::fluid).blast_resistance(500),
    blockproperties_t().id(BlockID::sand).texture(18).sound(SoundType::sand).fall(true),
    blockproperties_t().id(BlockID::gravel).texture(19).sound(SoundType::dirt).fall(true),
    blockproperties_t().id(BlockID::gold_ore).texture(32).sound(SoundType::stone),
    blockproperties_t().id(BlockID::iron_ore).texture(33).sound(SoundType::stone),
    blockproperties_t().id(BlockID::coal_ore).texture(34).sound(SoundType::stone),
    blockproperties_t().id(BlockID::wood).texture(20).sound(SoundType::wood).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::leaves).texture(186).sound(SoundType::grass).opacity(1).transparent(true),
    blockproperties_t().id(BlockID::sponge).texture(48).sound(SoundType::grass),
    blockproperties_t().id(BlockID::glass).texture(49).sound(SoundType::glass).opacity(0).transparent(true),
    blockproperties_t().id(BlockID::lapis_ore).texture(144).sound(SoundType::stone),
    blockproperties_t().id(BlockID::lapis_block).texture(160).sound(SoundType::stone),
    blockproperties_t().id(BlockID::dispenser).texture(46).sound(SoundType::stone).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::sandstone).texture(176).sound(SoundType::stone).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::note_block).texture(74).sound(SoundType::wood),
    blockproperties_t().id(BlockID::bed).texture(134).transparent(true).solid(false).opacity(0).sound(SoundType::stone).render_type(RenderType::special).valid_item(false),
    blockproperties_t().id(BlockID::golden_rail).texture(163).opacity(0).transparent(true).solid(false).sound(SoundType::metal).render_type(RenderType::flat_ground).collision(CollisionType::none),
    blockproperties_t().id(BlockID::detector_rail).texture(179).opacity(0).transparent(true).solid(false).sound(SoundType::metal).render_type(RenderType::flat_ground).collision(CollisionType::none),
    blockproperties_t().id(BlockID::sticky_piston).state(0x0B).texture(106).solid(false).sound(SoundType::stone).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::cobweb).texture(11).solid(false).opacity(0).transparent(true).sound(SoundType::stone).render_type(RenderType::cross).collision(CollisionType::none),
    blockproperties_t().id(BlockID::deadbush).texture(55).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none),
    blockproperties_t().id(BlockID::piston).state(3).texture(108).solid(false).sound(SoundType::stone).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::piston_head).texture(107).solid(false).sound(SoundType::stone).valid_item(false),
    blockproperties_t().id(BlockID::wool).texture(64).sound(SoundType::cloth),
    blockproperties_t().id(BlockID::piston_extension).texture(0).solid(false).sound(SoundType::stone).valid_item(false),
    blockproperties_t().id(BlockID::dandelion).texture(13).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none),
    blockproperties_t().id(BlockID::rose).texture(12).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none),
    blockproperties_t().id(BlockID::brown_mushroom).texture(29).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none),
    blockproperties_t().id(BlockID::red_mushroom).texture(28).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none),
    blockproperties_t().id(BlockID::gold_block).texture(23).sound(SoundType::metal),
    blockproperties_t().id(BlockID::iron_block).texture(22).sound(SoundType::metal),
    blockproperties_t().id(BlockID::double_stone_slab).texture(6).sound(SoundType::stone).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::stone_slab).texture(6).opacity(0).solid(false).transparent(true).sound(SoundType::stone).aabb(slab_aabb).render_type(RenderType::slab),
    blockproperties_t().id(BlockID::bricks).texture(7).sound(SoundType::stone),
    blockproperties_t().id(BlockID::tnt).texture(8).sound(SoundType::grass).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::bookshelf).texture(35).sound(SoundType::wood).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::mossy_cobblestone).texture(36).sound(SoundType::stone),
    blockproperties_t().id(BlockID::obsidian).texture(37).sound(SoundType::stone).blast_resistance(-1),
    blockproperties_t().id(BlockID::torch).texture(80).solid(false).opacity(0).transparent(true).luminance(14).sound(SoundType::wood).render_type(RenderType::special).collision(CollisionType::none),
    blockproperties_t().id(BlockID::fire).texture(31).solid(false).opacity(0).transparent(true).luminance(15).sound(SoundType::cloth),
    blockproperties_t().id(BlockID::mob_spawner).texture(65).solid(false).opacity(0).transparent(true).sound(SoundType::metal),
    blockproperties_t().id(BlockID::oak_stairs).texture(4).solid(false).sound(SoundType::wood),
    blockproperties_t().id(BlockID::chest).texture(27).sound(SoundType::wood).render_type(RenderType::full_special),
    blockproperties_t().id(BlockID::redstone_wire).texture(164).solid(false).opacity(0).transparent(true).sound(SoundType::stone).render_type(RenderType::flat_ground).valid_item(false).collision(CollisionType::none),
    blockproperties_t().id(BlockID::diamond_ore).texture(50).sound(SoundType::stone),
    blockproperties_t().id(BlockID::diamond_block).texture(24).sound(SoundType::metal),

    // Reserved for a fully white block.
    blockproperties_t().id(BlockID::reserved).texture(0).solid(false).opacity(0).transparent(true).luminance(15).sound(SoundType::cloth),

};

sound_t get_step_sound(BlockID block_id)
{
    blockproperties_t properties = block_properties[int(block_id)];
    sound_t sound;
    switch (properties.m_sound_type)
    {
    case SoundType::cloth:
        sound = sound_t(get_random_sound(cloth_sounds));
        break;
    case SoundType::glass:
        sound = sound_t(get_random_sound(stone_sounds));
        break;
    case SoundType::grass:
        sound = sound_t(get_random_sound(grass_sounds));
        break;
    case SoundType::dirt:
        sound = sound_t(get_random_sound(gravel_sounds));
        break;
    case SoundType::sand:
        sound = sound_t(get_random_sound(sand_sounds));
        break;
    case SoundType::wood:
        sound = sound_t(get_random_sound(wood_sounds));
        break;
    case SoundType::stone:
    case SoundType::metal: // The metal sound is the same as stone with a different pitch.
        sound = sound_t(get_random_sound(stone_sounds));
        break;
    default:
        break;
    }
    sound.volume = 0.15f;

    if (properties.m_sound_type == SoundType::metal)
        sound.pitch = 1.5f;

    return sound;
}

sound_t get_mine_sound(BlockID block_id)
{
    blockproperties_t properties = block_properties[int(block_id)];
    sound_t sound;
    switch (properties.m_sound_type)
    {
    case SoundType::cloth:
        sound = sound_t(get_random_sound(cloth_sounds));
        break;
    case SoundType::glass:
        sound = sound_t(get_random_sound(stone_sounds));
        break;
    case SoundType::grass:
        sound = sound_t(get_random_sound(grass_sounds));
        break;
    case SoundType::dirt:
        sound = sound_t(get_random_sound(gravel_sounds));
        break;
    case SoundType::sand:
        sound = sound_t(get_random_sound(sand_sounds));
        break;
    case SoundType::wood:
        sound = sound_t(get_random_sound(wood_sounds));
        break;
    case SoundType::stone:
    case SoundType::metal: // The metal sound is the same as stone with a different pitch.
        sound = sound_t(get_random_sound(stone_sounds));
        break;
    default:
        break;
    }

    if (properties.m_sound_type == SoundType::metal)
        sound.pitch = 1.5f;

    return sound;
}

sound_t get_break_sound(BlockID block_id)
{
    blockproperties_t properties = block_properties[int(block_id)];
    sound_t sound;
    switch (properties.m_sound_type)
    {
    case SoundType::cloth:
        sound = sound_t(get_random_sound(cloth_sounds));
        break;
    case SoundType::glass:
        sound = sound_t(get_random_sound(glass_sounds));
        break;
    case SoundType::grass:
        sound = sound_t(get_random_sound(grass_sounds));
        break;
    case SoundType::dirt:
        sound = sound_t(get_random_sound(gravel_sounds));
        break;
    case SoundType::sand:
        sound = sound_t(get_random_sound(sand_sounds));
        break;
    case SoundType::wood:
        sound = sound_t(get_random_sound(wood_sounds));
        break;
    case SoundType::stone:
    case SoundType::metal: // The metal sound is the same as stone with a different pitch.
        sound = sound_t(get_random_sound(stone_sounds));
        break;
    default:
        break;
    }

    if (properties.m_sound_type == SoundType::metal)
        sound.pitch = 1.5f;

    return sound;
}

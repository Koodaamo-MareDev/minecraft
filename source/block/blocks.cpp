#include "blocks.hpp"

#include <math/vec3i.hpp>
#include <cmath>
#include <ported/Random.hpp>

#include "block_properties.hpp"
#include <world/light.hpp>
#include <sounds.hpp>
#include <world/world.hpp>

// Used for the block breaking animation
// If negative, use default texture index
// If positive, use this texture index for all blocks
static int32_t block_texture_index = -1;

int8_t get_block_opacity(BlockID blockid)
{
    return block_properties[int(blockid)].m_opacity;
}

bool is_solid(BlockID block_id)
{
    return block_properties[uint8_t(block_id)].m_solid;
}

uint8_t get_block_luminance(BlockID block_id)
{
    return block_properties[uint8_t(block_id)].m_luminance;
}

void override_texture_index(int32_t texture_index)
{
    block_texture_index = texture_index;
}

uint32_t get_default_texture_index(BlockID blockid)
{
    return block_texture_index >= 0 ? (block_texture_index & 0xFF) : block_properties[uint8_t(blockid)].m_texture_index;
}

uint32_t get_face_texture_index(Block *block, int face)
{
    if (!block)
        return 0;

    if (block_texture_index >= 0)
    {
        // If a custom texture index is set, use it for all blocks
        return (block_texture_index & 0xFF);
    }

    const int directionmap[] = {
        FACE_NZ,
        FACE_PX,
        FACE_PZ,
        FACE_NX};

    const int facingmap[] = {
        FACE_NY,
        FACE_PY,
        FACE_NZ,
        FACE_PZ,
        FACE_NX,
        FACE_PX,
    };

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
            return block->meta == 1 ? 68 : 202;
        case FACE_NY:
            return 2;
        default:
            return 203;
        }
    }
    case BlockID::leaves:
    {
        uint8_t meta = block->meta;
        if (meta == 1)
            return 219;
        if (meta == 2)
            return 52;
        return 186;
    }
    case BlockID::wood:
    { // log
        switch (face)
        {
        case FACE_NY:
        case FACE_PY:
            return 21;
        default:
            if (block->meta == 1)
                return 116;
            else if (block->meta == 2)
                return 117;
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
    case BlockID::sandstone:
    {
        if (face == FACE_NY)
            return 208;
        if (face == FACE_PY)
            return 176;
        return 192;
    }
    case BlockID::bookshelf:
    {
        if (face == FACE_NY || face == FACE_PY)
            return 4;
        return 35;
    }
    case BlockID::chest:
    {
        int block_direction = block->meta % 6;
        if (face == FACE_NY || face == FACE_PY)
            return 25;
        if (face == facingmap[block_direction])
            return 27;
        return 26;
    }
    case BlockID::double_stone_slab:
    {
        int variant = block->meta & 0x03;
        switch (variant)
        {
        case 0: // Stone
        {
            if (face == FACE_NY || face == FACE_PY)
                return 6;
            return 5;
        }
        case 1: // Sandstone
        {
            if (face == FACE_NY)
                return 208;
            if (face == FACE_PY)
                return 176;
            return 192;
        }
        case 2: // Wooden
            return 4;
        case 3: // Cobblestone
            return 16;
        default:
            return 6;
        }
    }
    case BlockID::crafting_table:
    {
        if (face == FACE_NY)
            return 4;
        if (face == FACE_PY)
            return 43;
        if (face == FACE_NX || face == FACE_NZ)
            return 59;
        return 60;
    }
    case BlockID::dispenser:
    case BlockID::furnace:
    case BlockID::lit_furnace:
    {
        int block_direction = block->meta % 6;
        if (face == FACE_NY || face == FACE_PY)
            return blockid == BlockID::dispenser ? 62 : 0;
        if (face == facingmap[block_direction])
            return get_default_texture_index(blockid);
        return 45;
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
    case BlockID::wool:
    {
        int color = block->meta & 0x0F;
        if (!color)
            return 64;
        return 226 - (((color & 0x07) << 4) | (color >> 3));
        break;
    }
    case BlockID::pumpkin:
    case BlockID::lit_pumpkin:
    {
        if (face == FACE_NY || face == FACE_PY)
            return 102;
        int block_direction = (block->meta) & 0x03;
        if (face == directionmap[block_direction])
            return blockid == BlockID::lit_pumpkin ? 120 : 119;
        return 118;
    }
    default:
        return get_default_texture_index(blockid);
    }
}

// Returns the BlockID of the fluid that the target fluid will
// convert to when target fluid attempts to override the original fluid.
BlockID fluid_collision_result(BlockID original, BlockID target)
{
    if (can_fluid_replace(target, original))
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

void schedule_update(World *world, const Vec3i &pos, Block &block)
{
    BlockProperties &props = properties(block.get_blockid());
    world->schedule_block_update(pos, block.get_blockid(), props.m_tick_rate);
}

int get_fluid_level_at(World *world, Vec3i pos, BlockID src_id)
{
    Block *block = world->get_block_at(pos);
    if (block && is_same_fluid(block->get_blockid(), src_id))
        return block->meta & 0xF;
    return -1;
}

int get_min_fluid_level(World *world, Vec3i pos, BlockID src_id, int src_fluid_level, int &sources)
{
    int fluid_level = get_fluid_level_at(world, pos, src_id);

    // If the fluid level is invalid, default to the original fluid level
    if (fluid_level < 0)
        return src_fluid_level;

    // If the fluid level is a source block, return 0
    if (fluid_level == 0)
        sources++;

    if (fluid_level >= 8)
        fluid_level = 0;

    if (src_fluid_level >= 0 && fluid_level >= src_fluid_level)
        return src_fluid_level;

    return fluid_level;
}

void set_fluid_stationary(World *world, Vec3i pos)
{
    // Set the block to the base fluid, keeping the same meta
    world->set_block_and_meta_at(pos, basefluid(world->get_block_id_at(pos)), world->get_meta_at(pos));
}

void flow_fluid_later(World *world, Vec3i pos)
{
    Block *block = world->get_block_at(pos);
    if (block)
    {
        world->set_block_and_meta_at(pos, flowfluid(world->get_block_id_at(pos)), world->get_meta_at(pos));
        schedule_update(world, pos, *block);
    }
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
float get_fluid_height(World *world, Vec3i pos, BlockID block_type)
{
    int block_x = pos.x, block_y = pos.y, block_z = pos.z;
    int surrounding_water = 0;
    float water_percentage = 0.0F;

    for (int i = 0; i < 4; ++i)
    {
        int check_x = block_x - (i & 1);
        int check_z = block_z - (i >> 1 & 1);
        if (is_same_fluid(world->get_block_id_at(Vec3i(check_x, block_y + 1, check_z), BlockID::air), block_type))
        {
            return 1.0F;
        }

        BlockID check_block_type = world->get_block_id_at(Vec3i(check_x, block_y, check_z), BlockID::air);
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
            int fluid_level = get_fluid_meta_level(world->get_block_at(Vec3i(check_x, block_y, check_z)));
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

bool can_any_fluid_replace(BlockID id)
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

bool can_fluid_replace(BlockID fluid, BlockID id)
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

int get_flow_weight(World *world, Vec3i pos, BlockID fluid_type, int accumulated_weight, int previous_direction)
{
    int min_weight = 1000;
    for (int i = 0; i < 4; i++)
    {
        if (!((i != 0 || previous_direction != 1) &&
              (i != 1 || previous_direction != 0) &&
              (i != 2 || previous_direction != 3) &&
              (i != 3 || previous_direction != 2)))
            continue;
        // if ((i ^ previous_direction) != 1) // Don't go back the way we came
        //     continue;

        int neighbor_x = pos.x;
        int neighbor_z = pos.z;

        if (i == 0)
            neighbor_x--;
        else if (i == 1)
            neighbor_x++;
        else if (i == 2)
            neighbor_z--;
        else if (i == 3)
            neighbor_z++;
        Block *neighbor = world->get_block_at(Vec3i(neighbor_x, pos.y, neighbor_z));
        if (!neighbor)
            continue;
        BlockID neighbor_id = neighbor->get_blockid();

        if (can_any_fluid_replace(neighbor_id) && (is_same_fluid(neighbor_id, fluid_type) || neighbor->meta != 0))
        {
            // Check if the neighbor allows downward flow
            if (can_any_fluid_replace(world->get_block_id_at(Vec3i(neighbor_x, pos.y - 1, neighbor_z), BlockID::stone)))
                return accumulated_weight;
            if (accumulated_weight >= 4)
                continue;
            int weight = get_flow_weight(world, Vec3i(neighbor_x, pos.y, neighbor_z), fluid_type, accumulated_weight + 1, i);
            if (weight < min_weight)
                min_weight = weight;
        }
    }
    return min_weight;
}

void get_flow_directions(World *world, Vec3i pos, BlockID fluid_type, bool directions[4])
{
    int weights[4];
    for (int i = 0; i < 4; i++)
    {
        weights[i] = 1000;
        int neighbor_x = pos.x;
        int neighbor_z = pos.z;

        if (i == 0)
            neighbor_x--;
        else if (i == 1)
            neighbor_x++;
        else if (i == 2)
            neighbor_z--;
        else if (i == 3)
            neighbor_z++;
        Block *neighbor = world->get_block_at(Vec3i(neighbor_x, pos.y, neighbor_z));
        if (!neighbor)
            continue;
        BlockID neighbor_id = neighbor->get_blockid();
        if (can_any_fluid_replace(neighbor_id) && (basefluid(neighbor_id) != basefluid(fluid_type) || neighbor->meta != 0))
        {
            if (can_any_fluid_replace(world->get_block_id_at(Vec3i(neighbor_x, pos.y - 1, neighbor_z), BlockID::stone)))
                weights[i] = 0;
            else
                weights[i] = get_flow_weight(world, Vec3i(neighbor_x, pos.y, neighbor_z), fluid_type, 1, i);
        }
    }

    int min_weight = weights[0];
    for (int i = 1; i < 4; i++)
    {
        if (weights[i] < min_weight)
            min_weight = weights[i];
    }

    for (int i = 0; i < 4; i++)
    {
        directions[i] = (weights[i] == min_weight);
    }
}

// TODO: Replace with get_fluid_level
uint8_t get_fluid_meta_level(Block *block)
{
    if (block)
    {
        BlockID block_id = block->get_blockid();
        if (is_fluid(block_id))
            return block->meta & 0xF;
    }
    return 8;
}

uint8_t get_fluid_visual_level(World *world, Vec3i pos, BlockID block_id)
{
    return FLOAT_TO_FLUIDMETA(get_fluid_height(world, pos, block_id));
}

inventory::ItemStack default_drop(const Block &old_block)
{
    return inventory::ItemStack(old_block.id, 1, old_block.meta);
}

inventory::ItemStack fixed_drop(const Block &old_block, inventory::ItemStack item)
{
    return item;
}

inventory::ItemStack no_drop(const Block &old_block)
{
    return inventory::ItemStack(BlockID::air, 0);
}

// Randomly drops items with a count between 1 and the original count.
inventory::ItemStack random_drop(const Block &old_block, inventory::ItemStack item)
{
    javaport::Random rng;
    if (item.count > 1)
        item.count = rng.nextInt(item.count) + 1;
    return item;
}

void default_destroy(World *world, const Vec3i &pos, const Block &old_block)
{
    Block *block = world->get_block_at(pos + Vec3i(0, 1, 0));
    if (block && properties(block->get_blockid()).m_needs_support)
    {
        // If the neighbor block needs support, destroy it.
        world->destroy_block(pos + Vec3i(0, 1, 0), block);
    }
}

void stationary_fluid_tick(World *world, const Vec3i &pos, Block &block)
{
    // TODO: Lava fire spread
}

void flowing_fluid_tick(World *world, const Vec3i &pos, Block &block)
{
    BlockID orig_id = block.get_blockid();
    BlockProperties props = properties(orig_id);
    int orig_level = get_fluid_level_at(world, pos, orig_id);
    uint8_t drain_rate = props.m_drain_rate;
    if (world->hell)
        drain_rate = std::max<uint8_t>(1, drain_rate / 2);

    bool stationary = true;

    if (orig_level > 0)
    {
        int surrounding_sources = 0;
        int min_surrounding_level = -100;

        // Calculate next fluid level
        min_surrounding_level = get_min_fluid_level(world, pos + Vec3i(-1, 0, 0), orig_id, min_surrounding_level, surrounding_sources);
        min_surrounding_level = get_min_fluid_level(world, pos + Vec3i(1, 0, 0), orig_id, min_surrounding_level, surrounding_sources);
        min_surrounding_level = get_min_fluid_level(world, pos + Vec3i(0, 0, -1), orig_id, min_surrounding_level, surrounding_sources);
        min_surrounding_level = get_min_fluid_level(world, pos + Vec3i(0, 0, 1), orig_id, min_surrounding_level, surrounding_sources);

        int current_level = min_surrounding_level + drain_rate;
        if (current_level >= 8 || min_surrounding_level < 0)
            current_level = -1;

        int level_above = get_fluid_level_at(world, pos + Vec3i(0, 1, 0), orig_id);
        if (level_above >= 0)
        {
            current_level = level_above | 8;
        }

        // Water source blocks can form if there are 2 or more surrounding source blocks
        if (surrounding_sources >= 2 && orig_id == BlockID::flowing_water)
        {
            BlockID below_id = world->get_block_id_at(pos + Vec3i(0, -1, 0), BlockID::air);

            if (is_solid(below_id) || (block.meta == 0 && basefluid(below_id) == orig_id))
                current_level = 0;
        }

        // Randomly slow down lava flow.
        if (orig_id == BlockID::flowing_lava && orig_level < 8 && current_level < 8 && current_level > orig_level)
        {
            javaport::Random rng;
            if (rng.nextInt(4) != 0)
            {
                current_level = orig_level;
                stationary = false;
            }
        }

        // Update the block if the fluid level changed.
        if (current_level != orig_level)
        {
            orig_level = current_level;
            if (current_level < 0)
            {
                // Drain the fluid completely.
                world->set_block_at(pos, BlockID::air);
                world->notify_at(pos);
            }
            else
            {
                // Update to the new fluid level.
                world->set_meta_at(pos, current_level);
                world->notify_at(pos);
                world->schedule_block_update(pos, orig_id, props.m_tick_rate);
            }
        }
        else if (stationary)
        {
            set_fluid_stationary(world, pos);
        }
    }
    else
    {
        set_fluid_stationary(world, pos);
    }

    auto try_flow = [&](Vec3i offset, int level) -> bool
    {
        if (can_fluid_replace(orig_id, world->get_block_id_at(pos + offset, BlockID::stone)))
        {
            world->set_block_and_meta_at(pos + offset, orig_id, level);
            world->notify_at(pos + offset);
            return true;
        }
        return false;
    };

    // Flow downwards if possible
    if (!try_flow(Vec3i(0, -1, 0), orig_level | 8) && (orig_level >= 0 && (orig_level == 0 || !can_any_fluid_replace(world->get_block_id_at(pos + Vec3i(0, -1, 0), BlockID::stone)))))
    {
        // Spread to sides if can't flow downwards
        bool flow_directions[4];
        get_flow_directions(world, pos, orig_id, flow_directions);

        int current_level = orig_level + drain_rate;

        // If the fluid level is a source block, it will flow out as level 1.
        if (orig_level >= 8)
            current_level = 1;

        // Don't flow if the "falling" flag is set.
        if (current_level >= 8)
            return;

        if (flow_directions[0])
        {
            try_flow(Vec3i(-1, 0, 0), current_level);
        }
        if (flow_directions[1])
        {
            try_flow(Vec3i(1, 0, 0), current_level);
        }
        if (flow_directions[2])
        {
            try_flow(Vec3i(0, 0, -1), current_level);
        }
        if (flow_directions[3])
        {
            try_flow(Vec3i(0, 0, 1), current_level);
        }
    }
}

void fluid_harden(World *world, const Vec3i &pos, Block &block)
{
    BlockID base_fluid_id = basefluid(block.get_blockid());
    if (base_fluid_id != BlockID::lava)
        return;

    for (int direction = 0; direction < 6; direction++)
    {
        if (direction == FACE_NY)
            continue;
        Vec3i offset = face_offsets[direction];
        BlockID neighbor_id = world->get_block_id_at(pos + offset, BlockID::air);
        if (is_same_fluid(neighbor_id, BlockID::water))
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

void fluid_neighbor_update(World *world, const Vec3i &pos, Block &block)
{
    fluid_harden(world, pos, block);
}

void stationary_fluid_neighbor_update(World *world, const Vec3i &pos, Block &block)
{
    BlockID old_id = block.get_blockid();
    fluid_neighbor_update(world, pos, block);
    if (block.id == old_id)
        flow_fluid_later(world, pos);
}

void fluid_added(World *world, const Vec3i &pos, Block &block)
{
    fluid_harden(world, pos, block);
}

void flowing_fluid_added(World *world, const Vec3i &pos, Block &block)
{
    BlockID old_id = block.get_blockid();
    fluid_added(world, pos, block);
    if (block.id == old_id)
        schedule_update(world, pos, block);
}

void falling_block_tick(World *world, const Vec3i &pos, Block &block)
{
    BlockID block_below = world->get_block_id_at(pos + Vec3i(0, -1, 0));
    if (block_below == BlockID::air || properties(block_below).m_fluid)
    {
        world->add_entity(new EntityFallingBlock(block, pos));
        world->set_block_and_meta_at(pos, BlockID::air, 0);
        world->notify_at(pos);
    }
}

void snow_layer_tick(World *world, const Vec3i &pos, Block &block)
{
    Block *block_below = world->get_block_at(pos + Vec3i(0, -1, 0));
    if (block_below && block_below->get_blockid() == BlockID::grass)
    {
        block_below->meta = 1; // Set the snowy flag
        world->mark_block_dirty(pos + Vec3i(0, -1, 0));
    }
}

// Turns broken ice into water unless it's floating in the air.
void melt_destroy(World *world, const Vec3i &pos, const Block &old_block)
{
    if (pos.y > 0)
    {
        if (world->get_block_id_at(pos - Vec3i(0, 1, 0), BlockID::air) != BlockID::air)
        {
            world->set_block_at(pos, BlockID::water);
        }
    }
}

void snow_layer_destroy(World *world, const Vec3i &pos, const Block &old_block)
{
    Block *block = world->get_block_at(pos - Vec3i(0, 1, 0));
    if (block && block->get_blockid() == BlockID::grass)
    {
        // If the block below is grass, reset the snowy flag.
        block->meta = 0;
        world->mark_block_dirty(pos - Vec3i(0, 1, 0));
    }
}

void spawn_tnt_destroy(World *world, const Vec3i &pos, const Block &old_block)
{
    world->add_entity(new EntityExplosiveBlock(old_block, pos, 80));
}

void default_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    aabb.min = Vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + Vec3f(1, 1, 1);
    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void slab_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;

    aabb.min = Vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + Vec3f(1, 1, 1);

    if ((block->meta & 8))
    {
        aabb.min.y += 0.5;
    }
    else
    {
        aabb.max.y -= 0.5;
    }

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void torch_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    constexpr vfloat_t offset = 0.35;
    constexpr vfloat_t width = 0.3;
    constexpr vfloat_t height = 0.6;
    aabb.min = Vec3f(pos.x + offset, pos.y + 0.2, pos.z + offset);
    aabb.max = aabb.min + Vec3f(width, height, width);
    switch (block->meta)
    {
    case 1:
        aabb.translate(Vec3f(-offset, 0, 0));
        break;
    case 2:
        aabb.translate(Vec3f(offset, 0, 0));
        break;
    case 3:
        aabb.translate(Vec3f(0, 0, -offset));
        break;
    case 4:
        aabb.translate(Vec3f(0, 0, offset));
        break;
    default:
        aabb.translate(Vec3f(0, -0.2, 0));
        break;
    }

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void mushroom_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    constexpr vfloat_t offset = 0.25;
    constexpr vfloat_t width = 0.5;
    constexpr vfloat_t height = 0.375;

    aabb.min = Vec3f(pos.x + offset, pos.y, pos.z + offset);
    aabb.max = aabb.min + Vec3f(width, height, width);

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void flower_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    constexpr vfloat_t offset = 0.3125;
    constexpr vfloat_t width = 0.375;
    constexpr vfloat_t height = 0.625;

    aabb.min = Vec3f(pos.x + offset, pos.y, pos.z + offset);
    aabb.max = aabb.min + Vec3f(width, height, width);

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void flat_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    constexpr vfloat_t width = 1.0;
    constexpr vfloat_t height = 0.0625;

    aabb.min = Vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + Vec3f(width, height, width);

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void snow_layer_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    constexpr vfloat_t width = 1.0;
    constexpr vfloat_t height = 0.125;

    aabb.min = Vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + Vec3f(width, height, width);

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void door_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    aabb.min = Vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + Vec3f(1, 1, 1);

    bool open = block->meta & 4;
    uint8_t direction = block->meta & 3;
    if (!open)
        direction = (direction + 3) & 3;

    constexpr vfloat_t door_thickness = 0.1875;

    if (direction == 0)
        aabb.max.z -= 1 - door_thickness;
    else if (direction == 1)
        aabb.min.x += 1 - door_thickness;
    else if (direction == 2)
        aabb.min.z += 1 - door_thickness;
    else if (direction == 3)
        aabb.max.x -= 1 - door_thickness;

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void cactus_aabb(const Vec3i &pos, Block *block, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    constexpr vfloat_t width = 0.875;
    constexpr vfloat_t height = 1.0;
    aabb.min = Vec3f(pos.x + 0.0625, pos.y, pos.z + 0.0625);
    aabb.max = aabb.min + Vec3f(width, height, width);

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

BlockProperties block_properties[256] = {
    BlockProperties().id(BlockID::air).opacity(0).solid(false).transparent(true).collision(CollisionType::none).valid_item(false),
    BlockProperties().id(BlockID::stone).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(1.5f).texture(0).sound(SoundType::stone).drops(std::bind(fixed_drop, std::placeholders::_1, inventory::ItemStack(BlockID::cobblestone, 1))),
    BlockProperties().id(BlockID::grass).tool(inventory::tool_type::shovel, inventory::tool_tier::no_tier).hardness(0.6f).texture(202).sound(SoundType::grass).render_type(RenderType::full_special).drops(std::bind(fixed_drop, std::placeholders::_1, inventory::ItemStack(BlockID::dirt, 1))),
    BlockProperties().id(BlockID::dirt).tool(inventory::tool_type::shovel, inventory::tool_tier::no_tier).hardness(0.5f).texture(2).sound(SoundType::dirt),
    BlockProperties().id(BlockID::cobblestone).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(2.0f).texture(16).sound(SoundType::stone),
    BlockProperties().id(BlockID::planks).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(2.0f).texture(4).sound(SoundType::wood),
    BlockProperties().id(BlockID::sapling).tool(inventory::tool_type::none, inventory::tool_tier::no_tier).hardness(0.0f).texture(15).sound(SoundType::grass).opacity(0).solid(false).transparent(true).render_type(RenderType::cross).collision(CollisionType::none),
    BlockProperties().id(BlockID::bedrock).tool(inventory::tool_type::pickaxe, inventory::tool_tier::diamond).hardness(-1.0f).texture(17).sound(SoundType::stone).blast_resistance(-1),
    BlockProperties().id(BlockID::flowing_water).hardness(100.0f).texture(206).fluid(true).base_fluid(BlockID::water).flow_fluid(BlockID::flowing_water).drain_rate(1).neighbor_changed(fluid_neighbor_update).added(flowing_fluid_added).tick_on_load(true).tick_rate(5).tick(flowing_fluid_tick).opacity(1).solid(false).transparent(true).render_type(RenderType::special).collision(CollisionType::fluid).blast_resistance(500),
    BlockProperties().id(BlockID::water).hardness(100.0f).texture(205).fluid(true).base_fluid(BlockID::water).flow_fluid(BlockID::flowing_water).drain_rate(1).neighbor_changed(stationary_fluid_neighbor_update).added(fluid_added).opacity(1).solid(false).transparent(true).render_type(RenderType::special).collision(CollisionType::fluid).blast_resistance(500),
    BlockProperties().id(BlockID::flowing_lava).hardness(0.0f).texture(238).fluid(true).base_fluid(BlockID::lava).flow_fluid(BlockID::flowing_lava).drain_rate(2).neighbor_changed(fluid_neighbor_update).added(flowing_fluid_added).tick_on_load(true).tick_rate(30).tick(flowing_fluid_tick).opacity(1).luminance(15).solid(false).render_type(RenderType::special).collision(CollisionType::fluid).blast_resistance(500),
    BlockProperties().id(BlockID::lava).hardness(100.0f).texture(237).fluid(true).base_fluid(BlockID::lava).flow_fluid(BlockID::flowing_lava).drain_rate(2).neighbor_changed(stationary_fluid_neighbor_update).added(fluid_added).tick_on_load(true).opacity(1).luminance(15).solid(false).render_type(RenderType::special).collision(CollisionType::fluid).blast_resistance(500),
    BlockProperties().id(BlockID::sand).tool(inventory::tool_type::shovel, inventory::tool_tier::no_tier).hardness(0.5f).texture(18).sound(SoundType::sand).tick_rate(3).tick(falling_block_tick).added(schedule_update).neighbor_changed(schedule_update),
    BlockProperties().id(BlockID::gravel).tool(inventory::tool_type::shovel, inventory::tool_tier::no_tier).hardness(0.6f).texture(19).sound(SoundType::dirt).tick_rate(3).tick(falling_block_tick).added(schedule_update).neighbor_changed(schedule_update),
    BlockProperties().id(BlockID::gold_ore).tool(inventory::tool_type::pickaxe, inventory::tool_tier::stone).hardness(3.0f).texture(32).sound(SoundType::stone),
    BlockProperties().id(BlockID::iron_ore).tool(inventory::tool_type::pickaxe, inventory::tool_tier::stone).hardness(3.0f).texture(33).sound(SoundType::stone),
    BlockProperties().id(BlockID::coal_ore).tool(inventory::tool_type::pickaxe, inventory::tool_tier::stone).hardness(3.0f).texture(34).sound(SoundType::stone),
    BlockProperties().id(BlockID::wood).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(2.0f).texture(20).sound(SoundType::wood).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::leaves).hardness(0.2f).texture(186).sound(SoundType::grass).opacity(1).transparent(true).render_type(RenderType::special).nonflat(true),
    BlockProperties().id(BlockID::sponge).hardness(0.6f).texture(48).sound(SoundType::grass),
    BlockProperties().id(BlockID::glass).hardness(0.3f).texture(49).sound(SoundType::glass).opacity(0).transparent(true).drops(std::bind(no_drop, std::placeholders::_1)),
    BlockProperties().id(BlockID::lapis_ore).tool(inventory::tool_type::pickaxe, inventory::tool_tier::stone).hardness(3.0f).texture(160).sound(SoundType::stone),
    BlockProperties().id(BlockID::lapis_block).tool(inventory::tool_type::pickaxe, inventory::tool_tier::stone).hardness(3.0f).texture(144).sound(SoundType::stone),
    BlockProperties().id(BlockID::dispenser).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(3.5f).texture(46).sound(SoundType::stone).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::sandstone).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(0.8f).texture(176).sound(SoundType::stone).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::note_block).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(0.8f).texture(74).sound(SoundType::wood),
    BlockProperties().id(BlockID::bed).hardness(0.2f).texture(134).transparent(true).solid(false).opacity(0).sound(SoundType::stone).render_type(RenderType::special).valid_item(false),
    BlockProperties().id(BlockID::golden_rail).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).texture(163).opacity(0).transparent(true).solid(false).sound(SoundType::metal).aabb(flat_aabb).render_type(RenderType::flat_ground).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::detector_rail).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).texture(179).opacity(0).transparent(true).solid(false).sound(SoundType::metal).aabb(flat_aabb).render_type(RenderType::flat_ground).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::sticky_piston).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).state(0x0B).texture(106).solid(false).sound(SoundType::stone).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::cobweb).tool(inventory::tool_type::sword, inventory::tool_tier::wood).texture(11).solid(false).opacity(0).transparent(true).sound(SoundType::stone).render_type(RenderType::cross).collision(CollisionType::none),
    BlockProperties().id(BlockID::tallgrass).texture(55).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::deadbush).texture(55).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::piston).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).state(3).texture(108).solid(false).sound(SoundType::stone).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::piston_head).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).texture(107).solid(false).sound(SoundType::stone).valid_item(false),
    BlockProperties().id(BlockID::wool).hardness(0.8f).texture(64).sound(SoundType::cloth).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::piston_extension).texture(0).solid(false).sound(SoundType::stone).valid_item(false),
    BlockProperties().id(BlockID::dandelion).hardness(0.0f).texture(13).solid(false).opacity(0).transparent(true).sound(SoundType::grass).aabb(flower_aabb).render_type(RenderType::cross).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::rose).hardness(0.0f).texture(12).solid(false).opacity(0).transparent(true).sound(SoundType::grass).aabb(flower_aabb).render_type(RenderType::cross).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::brown_mushroom).hardness(0.0f).texture(29).solid(false).opacity(0).transparent(true).sound(SoundType::grass).aabb(mushroom_aabb).render_type(RenderType::cross).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::red_mushroom).hardness(0.0f).texture(28).solid(false).opacity(0).transparent(true).sound(SoundType::grass).aabb(mushroom_aabb).render_type(RenderType::cross).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::gold_block).tool(inventory::tool_type::pickaxe, inventory::tool_tier::iron).hardness(3.0f).texture(23).sound(SoundType::metal),
    BlockProperties().id(BlockID::iron_block).tool(inventory::tool_type::pickaxe, inventory::tool_tier::iron).hardness(5.0f).texture(22).sound(SoundType::metal),
    BlockProperties().id(BlockID::double_stone_slab).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(2.0f).texture(6).sound(SoundType::stone).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::stone_slab).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(2.0f).texture(6).opacity(0).solid(false).transparent(true).sound(SoundType::stone).aabb(slab_aabb).render_type(RenderType::slab),
    BlockProperties().id(BlockID::bricks).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(2.0f).texture(7).sound(SoundType::stone),
    BlockProperties().id(BlockID::tnt).hardness(0.0f).texture(8).sound(SoundType::grass).render_type(RenderType::full_special).destroy(spawn_tnt_destroy).drops(std::bind(no_drop, std::placeholders::_1)),
    BlockProperties().id(BlockID::bookshelf).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(1.5f).texture(35).sound(SoundType::wood).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::mossy_cobblestone).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(2.0f).texture(36).sound(SoundType::stone),
    BlockProperties().id(BlockID::obsidian).tool(inventory::tool_type::pickaxe, inventory::tool_tier::diamond).hardness(10.0f).texture(37).sound(SoundType::stone).blast_resistance(-1),
    BlockProperties().id(BlockID::torch).hardness(0.0f).texture(80).solid(false).opacity(0).transparent(true).luminance(14).sound(SoundType::wood).aabb(torch_aabb).render_type(RenderType::special).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::fire).hardness(0.0f).texture(31).solid(false).opacity(0).transparent(true).luminance(15).sound(SoundType::cloth),
    BlockProperties().id(BlockID::mob_spawner).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(5.0f).texture(65).solid(false).opacity(0).transparent(true).sound(SoundType::metal).drops(std::bind(no_drop, std::placeholders::_1)),
    BlockProperties().id(BlockID::oak_stairs).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(2.0f).texture(4).solid(false).sound(SoundType::wood),
    BlockProperties().id(BlockID::chest).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(2.5f).texture(27).sound(SoundType::wood).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::redstone_wire).hardness(0.0f).texture(164).solid(false).opacity(0).transparent(true).sound(SoundType::stone).aabb(flat_aabb).render_type(RenderType::flat_ground).valid_item(false).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::diamond_ore).tool(inventory::tool_type::pickaxe, inventory::tool_tier::iron).hardness(3.0f).texture(50).sound(SoundType::stone),
    BlockProperties().id(BlockID::diamond_block).tool(inventory::tool_type::pickaxe, inventory::tool_tier::iron).hardness(5.0f).texture(24).sound(SoundType::metal),
    BlockProperties().id(BlockID::crafting_table).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(2.5f).texture(59).sound(SoundType::wood).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::wheat).hardness(0.0f).texture(95).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::farmland).tool(inventory::tool_type::shovel, inventory::tool_tier::no_tier).hardness(0.6f).texture(86).sound(SoundType::dirt).drops(std::bind(fixed_drop, std::placeholders::_1, inventory::ItemStack(BlockID::dirt, 1))),
    BlockProperties().id(BlockID::furnace).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(3.5f).texture(44).sound(SoundType::stone).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::lit_furnace).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(3.5f).texture(61).sound(SoundType::stone).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::standing_sign).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(1.0f).texture(4).solid(false).opacity(0).transparent(false).sound(SoundType::wood).render_type(RenderType::cross).collision(CollisionType::none),
    BlockProperties().id(BlockID::wooden_door).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(3.0f).texture(81).solid(false).opacity(0).transparent(true).sound(SoundType::wood).render_type(RenderType::special).aabb(door_aabb),
    BlockProperties().id(BlockID::ladder).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(0.4f).texture(83).solid(false).opacity(0).transparent(true).sound(SoundType::wood).render_type(RenderType::special).collision(CollisionType::none),
    BlockProperties().id(BlockID::rail).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(0.7f).texture(128).opacity(0).transparent(true).solid(false).sound(SoundType::metal).aabb(flat_aabb).render_type(RenderType::flat_ground).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::stone_stairs).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(2.0f).texture(16).solid(false).sound(SoundType::stone),
    BlockProperties().id(BlockID::wall_sign).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(1.0f).texture(4).solid(false).opacity(0).transparent(false).sound(SoundType::wood).render_type(RenderType::cross).collision(CollisionType::none),
    BlockProperties().id(BlockID::lever).hardness(0.5f).texture(96).solid(false).opacity(0).transparent(true).sound(SoundType::wood).render_type(RenderType::special).collision(CollisionType::none).aabb(flat_aabb),
    BlockProperties().id(BlockID::stone_pressure_plate).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(0.5f).texture(0).solid(false).opacity(0).transparent(true).sound(SoundType::stone).aabb(flat_aabb).render_type(RenderType::flat_ground).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::iron_door).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(5.0f).texture(82).solid(false).opacity(0).transparent(true).sound(SoundType::metal).render_type(RenderType::special).aabb(door_aabb),
    BlockProperties().id(BlockID::wooden_pressure_plate).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(0.5f).texture(4).solid(false).opacity(0).transparent(true).sound(SoundType::wood).aabb(flat_aabb).render_type(RenderType::flat_ground).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::redstone_ore).tool(inventory::tool_type::pickaxe, inventory::tool_tier::iron).hardness(3.0f).texture(51).sound(SoundType::stone),
    BlockProperties().id(BlockID::lit_redstone_ore).tool(inventory::tool_type::pickaxe, inventory::tool_tier::iron).texture(51).sound(SoundType::stone).luminance(9),
    BlockProperties().id(BlockID::unlit_redstone_torch).hardness(0.0f).texture(115).solid(false).opacity(0).transparent(true).luminance(0).sound(SoundType::stone).aabb(torch_aabb).render_type(RenderType::special).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::redstone_torch).hardness(0.0f).texture(99).solid(false).opacity(0).transparent(true).luminance(7).sound(SoundType::stone).aabb(torch_aabb).render_type(RenderType::special).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::stone_button).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(0.5f).texture(0).solid(false).opacity(0).transparent(false).sound(SoundType::stone).aabb(flat_aabb).render_type(RenderType::special).collision(CollisionType::none),
    BlockProperties().id(BlockID::snow_layer).tool(inventory::tool_type::shovel, inventory::tool_tier::wood).hardness(0.1f).texture(66).solid(false).opacity(0).transparent(false).sound(SoundType::cloth).aabb(snow_layer_aabb).render_type(RenderType::special).collision(CollisionType::none).drops(std::bind(no_drop, std::placeholders::_1)).destroy(snow_layer_destroy).needs_support(true),
    BlockProperties().id(BlockID::ice).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(0.5f).texture(67).solid(true).opacity(3).transparent(true).sound(SoundType::glass).slipperiness(0.98f).drops(std::bind(no_drop, std::placeholders::_1)).destroy(std::bind(melt_destroy, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
    BlockProperties().id(BlockID::snow_block).tool(inventory::tool_type::shovel, inventory::tool_tier::wood).hardness(0.2f).texture(66).solid(false).opacity(0).transparent(true).sound(SoundType::cloth).render_type(RenderType::full).collision(CollisionType::none),
    BlockProperties().id(BlockID::cactus).hardness(0.4f).texture(70).solid(false).opacity(0).transparent(true).sound(SoundType::cloth).aabb(cactus_aabb).render_type(RenderType::special).nonflat(true),
    BlockProperties().id(BlockID::clay).tool(inventory::tool_type::shovel, inventory::tool_tier::no_tier).hardness(0.6f).texture(72).sound(SoundType::dirt),
    BlockProperties().id(BlockID::reeds).hardness(0.0f).texture(73).solid(false).opacity(0).transparent(true).sound(SoundType::grass).render_type(RenderType::cross).collision(CollisionType::none).needs_support(true),
    BlockProperties().id(BlockID::jukebox).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(2.0f).texture(74).sound(SoundType::wood).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::fence).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(2.0f).texture(4).solid(false).opacity(0).transparent(true).sound(SoundType::wood).render_type(RenderType::special),
    BlockProperties().id(BlockID::pumpkin).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(1.0f).texture(118).opacity(0).sound(SoundType::wood).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::netherrack).tool(inventory::tool_type::pickaxe, inventory::tool_tier::wood).hardness(0.4f).texture(103).sound(SoundType::stone),
    BlockProperties().id(BlockID::soul_sand).tool(inventory::tool_type::shovel, inventory::tool_tier::no_tier).hardness(0.5f).texture(104).sound(SoundType::sand),
    BlockProperties().id(BlockID::glowstone).tool(inventory::tool_type::pickaxe, inventory::tool_tier::no_tier).hardness(0.3f).texture(105).sound(SoundType::glass).luminance(15),
    BlockProperties().id(BlockID::portal).hardness(-1.0f).texture(14).solid(false).opacity(0).transparent(true).luminance(11).sound(SoundType::glass).render_type(RenderType::special).collision(CollisionType::none),
    BlockProperties().id(BlockID::lit_pumpkin).tool(inventory::tool_type::axe, inventory::tool_tier::no_tier).hardness(1.0f).texture(120).opacity(0).sound(SoundType::wood).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::cake).hardness(0.5f).texture(121).opacity(0).sound(SoundType::wood).render_type(RenderType::full_special),
    BlockProperties().id(BlockID::unpowered_repeater).hardness(0.0f).texture(131).opacity(0).sound(SoundType::wood).render_type(RenderType::flat_ground),
    BlockProperties().id(BlockID::powered_repeater).hardness(0.0f).texture(147).opacity(0).sound(SoundType::wood).luminance(10).render_type(RenderType::flat_ground),
    // Reserved for a fully white block.
    BlockProperties().id(BlockID::reserved).texture(0).solid(false).opacity(0).transparent(true).luminance(15).sound(SoundType::cloth),

};

Sound get_step_sound(BlockID block_id)
{
    BlockProperties properties = block_properties[int(block_id)];
    Sound sound;
    switch (properties.m_sound_type)
    {
    case SoundType::cloth:
        sound = randomize_sound("step/cloth", 4);
        break;
    case SoundType::grass:
        sound = randomize_sound("step/grass", 4);
        break;
    case SoundType::dirt:
        sound = randomize_sound("step/gravel", 4);
        break;
    case SoundType::sand:
        sound = randomize_sound("step/sand", 4);
        break;
    case SoundType::wood:
        sound = randomize_sound("step/wood", 4);
        break;
    case SoundType::glass:
    case SoundType::stone:
    case SoundType::metal: // The metal sound is the same as stone with a different pitch.
        sound = randomize_sound("step/stone", 4);
        break;
    default:
        break;
    }
    sound.volume = 0.15f;

    if (properties.m_sound_type == SoundType::metal)
        sound.pitch = 1.5f;

    return sound;
}

Sound get_mine_sound(BlockID block_id)
{
    BlockProperties properties = block_properties[int(block_id)];
    Sound sound;
    switch (properties.m_sound_type)
    {
    case SoundType::cloth:
        sound = randomize_sound("step/cloth", 4);
        break;
    case SoundType::grass:
        sound = randomize_sound("step/grass", 4);
        break;
    case SoundType::dirt:
        sound = randomize_sound("step/gravel", 4);
        break;
    case SoundType::sand:
        sound = randomize_sound("step/sand", 4);
        break;
    case SoundType::wood:
        sound = randomize_sound("step/wood", 4);
        break;
    case SoundType::glass:
    case SoundType::stone:
    case SoundType::metal: // The metal sound is the same as stone with a different pitch.
        sound = randomize_sound("step/stone", 4);
        break;
    default:
        break;
    }

    if (properties.m_sound_type == SoundType::metal)
        sound.pitch = 1.5f;

    return sound;
}

Sound get_break_sound(BlockID block_id)
{
    BlockProperties properties = block_properties[int(block_id)];
    Sound sound;
    switch (properties.m_sound_type)
    {
    case SoundType::cloth:
        sound = randomize_sound("step/cloth", 4);
        break;
    case SoundType::glass:
        sound = randomize_sound("step/glass", 3);
        break;
    case SoundType::grass:
        sound = randomize_sound("step/grass", 4);
        break;
    case SoundType::dirt:
        sound = randomize_sound("step/gravel", 4);
        break;
    case SoundType::sand:
        sound = randomize_sound("step/gravel", 4);
        break;
    case SoundType::wood:
        sound = randomize_sound("step/wood", 4);
        break;
    case SoundType::stone:
    case SoundType::metal: // The metal sound is the same as stone with a different pitch.
        sound = randomize_sound("step/stone", 4);
        break;
    default:
        break;
    }

    if (properties.m_sound_type == SoundType::metal)
        sound.pitch = 1.5f;

    return sound;
}

#include "block_flowing.hpp"
#include <world/world.hpp>
#include <block/block_list.hpp>

BlockFlowing::BlockFlowing(uint16_t id, Materials material) : BlockFluids(id, material)
{
}

int BlockFlowing::get_flow_weight(World *world, Vec3i pos, BlockID fluid_type, int accumulated_weight, int previous_direction)
{
    int min_weight = 1000;
    for (int i = 0; i < 4; i++)
    {
        if (!((i != 0 || previous_direction != 1) &&
              (i != 1 || previous_direction != 0) &&
              (i != 2 || previous_direction != 3) &&
              (i != 3 || previous_direction != 2)))
            continue;

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
        BlockID neighbor_id = neighbor->blockid;

        if (can_any_fluid_replace(neighbor_id) && (block_list[+neighbor_id]->material_type() == data.material || neighbor->meta != 0))
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

void BlockFlowing::get_flow_directions(World *world, Vec3i pos, BlockID fluid_type, bool directions[4])
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
        BlockID neighbor_id = neighbor->blockid;
        if (can_any_fluid_replace(neighbor_id) && (block_list[+neighbor_id]->material_type() != data.material || neighbor->meta != 0))
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

void BlockFlowing::on_added(World *world, const Vec3i &pos)
{
    BlockID old_id = data.block_id;
    BlockFluids::on_added(world, pos);
    BlockID new_id = world->get_block_id_at(pos);
    if (new_id == old_id)
        world->schedule_block_update(pos, new_id, tick_rate());
}

void BlockFlowing::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    BlockID old_id = data.block_id;
    BlockProperties props = properties(old_id);
    int orig_level = get_fluid_level_at(world, pos);
    uint8_t drain_rate = props.m_drain_rate;
    if (world->hell)
        drain_rate = std::max<uint8_t>(1, drain_rate / 2);

    bool stationary = true;

    if (orig_level > 0)
    {
        int surrounding_sources = 0;
        int min_surrounding_level = -100;

        // Calculate next fluid level
        min_surrounding_level = get_min_fluid_level(world, pos + Vec3i(-1, 0, 0), min_surrounding_level, surrounding_sources);
        min_surrounding_level = get_min_fluid_level(world, pos + Vec3i(1, 0, 0), min_surrounding_level, surrounding_sources);
        min_surrounding_level = get_min_fluid_level(world, pos + Vec3i(0, 0, -1), min_surrounding_level, surrounding_sources);
        min_surrounding_level = get_min_fluid_level(world, pos + Vec3i(0, 0, 1), min_surrounding_level, surrounding_sources);

        int current_level = min_surrounding_level + drain_rate;
        if (current_level >= 8 || min_surrounding_level < 0)
            current_level = -1;

        int level_above = get_fluid_level_at(world, pos + Vec3i(0, 1, 0));
        if (level_above >= 0)
        {
            current_level = level_above | 8;
        }

        // Water source blocks can form if there are 2 or more surrounding source blocks
        if (surrounding_sources >= 2 && old_id == BlockID::flowing_water)
        {
            BlockBase *below = block_at(world, pos + Vec3i(0, -1, 0));
            if (below->is_opaque() || (below->material_type() == data.material && world->get_meta_at(pos + Vec3i(0, -1, 0)) == 0))
                current_level = 0;
        }

        // Randomly slow down lava flow.
        if (old_id == BlockID::flowing_lava && orig_level < 8 && current_level < 8 && current_level > orig_level)
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
                world->schedule_block_update(pos, old_id, props.m_tick_rate);
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
        if (can_fluid_replace(old_id, world->get_block_id_at(pos + offset, BlockID::stone)))
        {
            world->set_block_and_meta_at(pos + offset, old_id, level);
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
        get_flow_directions(world, pos, old_id, flow_directions);

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

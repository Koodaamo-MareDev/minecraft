#include "block_stationary.hpp"

#include <world/world.hpp>
#include <registry/block_list.hpp>

BlockStationary::BlockStationary(uint16_t id, Materials material) : BlockFluids(id, material)
{
    data.tick_on_load = material == Materials::LAVA;
}

void BlockStationary::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    if (data.material == Materials::LAVA)
    {
        int rolls = random.nextInt(3);

        for (int i = 0; i < rolls; i++)
        {
            Vec3i spread_pos = pos + Vec3i{random.nextInt(3) - 1, 1, random.nextInt(3) - 1};
            int spread_id = world->get_block_id_at(spread_pos);
            if (spread_id == 0)
            {
                if (block_at(world, spread_pos + Vec3i{-1, 0, 0})->material().can_burn ||
                    block_at(world, spread_pos + Vec3i{+1, 0, 0})->material().can_burn ||
                    block_at(world, spread_pos + Vec3i{0, -1, 0})->material().can_burn ||
                    block_at(world, spread_pos + Vec3i{0, +1, 0})->material().can_burn ||
                    block_at(world, spread_pos + Vec3i{0, 0, -1})->material().can_burn ||
                    block_at(world, spread_pos + Vec3i{0, 0, +1})->material().can_burn)
                {
                    world->set_block_and_meta_at(spread_pos, BlockID::fire, 0);
                    return;
                }
            }
            else if (block_list[spread_id]->material().is_solid)
            {
                return;
            }
        }
    }
}

void BlockStationary::flow_fluid_later(World *world, const Vec3i &pos)
{
    BlockState *block = world->get_block_at(pos);
    if (block)
    {
        BlockID new_id = block_list[block->blockid]->material_type() == Materials::WATER ? BlockID::flowing_water : BlockID::flowing_lava;
        world->set_block_and_meta_at(pos, new_id, world->get_meta_at(pos));
        world->schedule_block_update(pos, new_id, tick_rate());
    }
}

void BlockStationary::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    BlockID old_id = world->get_block_id_at(pos);
    harden(world, pos);
    if (world->get_block_id_at(pos) == old_id)
        flow_fluid_later(world, pos);
}

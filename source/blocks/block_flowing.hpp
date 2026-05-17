#pragma once

#include <blocks/block_fluids.hpp>

class BlockFlowing : public BlockFluids
{
public:
    BlockFlowing(uint16_t id, Materials material);

    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;

protected:
    int get_flow_weight(World *world, Vec3i pos, BlockID fluid_type, int accumulated_weight, int previous_direction);
    void get_flow_directions(World *world, Vec3i pos, BlockID fluid_type, bool directions[4]);
};
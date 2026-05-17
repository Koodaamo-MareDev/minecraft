#pragma once

#include <blocks/block_fluids.hpp>

class BlockStationary : public BlockFluids
{
public:
    BlockStationary(uint16_t id, Materials material);

    void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;

    void flow_fluid_later(World *world, const Vec3i &pos);
};
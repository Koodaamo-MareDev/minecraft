#pragma once

#include <block/block_base.hpp>

class BlockReeds : public BlockBase
{
public:
    BlockReeds(uint16_t id, uint8_t texture_index);
    bool can_place(World *world, const Vec3i &pos) override;
    bool can_stay(World *world, const Vec3i &pos) override;
    void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
    void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
};
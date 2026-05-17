#pragma once

#include <block/block_base.hpp>

class BlockFalling : public BlockBase
{
public:
    BlockFalling(uint16_t id, uint8_t texture_index, Materials material);

    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;

};
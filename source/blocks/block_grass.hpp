#pragma once

#include <block/block_base.hpp>

class BlockGrass : public BlockBase
{
public:
    BlockGrass(uint16_t id, uint8_t texture_index, Materials material);

    virtual uint8_t texture_index(World *world, const Vec3i &pos, uint8_t face) override;
    virtual void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
};
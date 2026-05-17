#pragma once

#include <blocks/block_shatterable.hpp>

class BlockIce : public BlockShatterable
{

public:
    BlockIce(uint16_t id, uint8_t texture_index, Materials material);

    virtual bool should_render_side(World *world, const Vec3i &pos, uint8_t face) override;
    virtual void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual void on_removed(World *world, const Vec3i &pos) override;
    virtual uint16_t drop_count(javaport::Random &random) override;
};
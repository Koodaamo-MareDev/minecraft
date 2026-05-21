#pragma once

#include <block/block_base.hpp>

class BlockSnowLayer : public BlockBase
{

public:
    BlockSnowLayer(uint16_t id, uint8_t texture_index, Materials material);
    virtual bool is_opaque() override;
    virtual bool should_render_side(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual bool can_stay(World *world, const Vec3i &pos) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
    virtual void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual void on_harvest(World *world, const Vec3i &pos, uint8_t meta) override;
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
    virtual uint16_t drop_count(javaport::Random &random) override;
};
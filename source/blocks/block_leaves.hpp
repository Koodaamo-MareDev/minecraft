#pragma once

#include <block/block_base.hpp>

class BlockLeaves : public BlockBase
{
public:
    BlockLeaves(uint16_t id, uint8_t texture_index);

    virtual bool is_opaque() override;
    virtual bool should_render_side(World *world, const Vec3i &pos, uint8_t face) override;
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;

    virtual void on_removed(World *world, const Vec3i &pos) override;
    virtual void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random) override;

    virtual uint16_t drop_count(javaport::Random &random) override;
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
};
#pragma once

#include <block/block_base.hpp>

class BlockSlab : public BlockBase
{
public:
    BlockSlab(uint16_t id, uint8_t texture_index, bool is_double_slab);
    virtual bool is_opaque() override;
    virtual bool should_render_side(World *world, const Vec3i &pos, uint8_t face) override;
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    virtual uint16_t drop_meta(uint16_t meta) override;
    virtual uint16_t drop_count(javaport::Random &random) override;
    virtual void get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;

private:
    bool is_double_slab;
};
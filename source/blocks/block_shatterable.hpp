#pragma once

#include <block/block_base.hpp>

class BlockShatterable : public BlockBase
{
public:
    BlockShatterable(uint16_t id, uint8_t texture_index, Materials material);

    virtual bool is_opaque() override;
    virtual bool should_render_side(World *world, const Vec3i &pos, uint8_t face) override;
};
#pragma once

#include <block/block_base.hpp>

class BlockBookshelf : public BlockBase
{
public:
    BlockBookshelf(uint16_t id, uint8_t texture_index);

    virtual uint8_t texture_index(World *world, const Vec3i &pos, uint8_t face) override;
    virtual uint16_t drop_count(javaport::Random &random) override;
};
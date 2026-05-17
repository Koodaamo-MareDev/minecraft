#pragma once

#include <block/block_base.hpp>

class BlockLog : public BlockBase
{
public:
    BlockLog(uint16_t id);

    uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    uint16_t drop_meta(uint16_t meta) override;
    void on_removed(World *world, const Vec3i &pos) override;
};
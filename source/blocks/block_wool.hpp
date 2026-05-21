#pragma once

#include <block/block_base.hpp>

class BlockWool : public BlockBase
{
public:
    BlockWool(uint16_t id, uint8_t texture_index);
    virtual uint16_t drop_meta(uint16_t meta) override;
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
};
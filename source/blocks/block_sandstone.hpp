#pragma once

#include <block/block_base.hpp>

class BlockSandStone : public BlockBase
{
public:
    BlockSandStone(uint16_t id, uint8_t texture_index, Materials material);
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;

};
#pragma once

#include <block/block_base.hpp>

class BlockGlowstone : public BlockBase
{
public:
    BlockGlowstone(uint16_t id, uint8_t texture_index, Materials material);

    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
};
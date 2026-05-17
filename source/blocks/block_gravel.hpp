#pragma once

#include <blocks/block_falling.hpp>

class BlockGravel : public BlockFalling
{
public:
    BlockGravel(uint16_t id, uint8_t texture_index, Materials material);

    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
};
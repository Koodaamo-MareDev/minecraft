#pragma once

#include <blocks/block_shatterable.hpp>

class BlockGlass : public BlockShatterable
{
public:
    BlockGlass(uint16_t id, uint8_t texture_index, Materials material);

    virtual uint16_t drop_count(javaport::Random &random) override;
};
#pragma once

#include <block/block_base.hpp>

class BlockClay : public BlockBase
{
public:
    BlockClay(uint16_t id, uint8_t texture_index);
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random);
    virtual uint16_t drop_count(javaport::Random &random);
};
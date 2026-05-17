#pragma once

#include <block/block_base.hpp>

class BlockOre : public BlockBase
{
public:
    BlockOre(uint16_t id, uint8_t texture_index);

    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
    virtual uint16_t drop_count(javaport::Random &random) override;
    virtual uint16_t drop_meta(uint16_t meta) override;
};

#pragma once

#include <blocks/block_container.hpp>

class BlockSpawner : public BlockContainer
{
public:
    BlockSpawner(uint16_t id, uint8_t texture_index);

    virtual bool is_opaque() override;
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
    virtual uint16_t drop_count(javaport::Random &random) override;
    virtual TileEntity *get_tile_entity(World *world, const Vec3i &pos) override;
};
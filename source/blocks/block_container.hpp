#pragma once

#include <block/block_base.hpp>
#include <world/tile_entity/tile_entity.hpp>

class BlockContainer : public BlockBase
{
public:
    BlockContainer(uint16_t id, Materials material);
    BlockContainer(uint16_t id, uint8_t texture_index, Materials material);

    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_removed(World *world, const Vec3i &pos) override;

    virtual TileEntity *get_tile_entity(World *world, const Vec3i &pos) = 0;
};
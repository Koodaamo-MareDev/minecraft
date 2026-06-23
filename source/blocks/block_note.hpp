#pragma once

#include <blocks/block_container.hpp>

class BlockNote : public BlockContainer
{
public:
    BlockNote(uint16_t id);

    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id) override;
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos) override;
    virtual void on_click(EntityPhysical *entity, const Vec3i &pos) override;

    virtual TileEntity *get_tile_entity(World *world, const Vec3i &pos) override;
};
#pragma once

#include "block_container.hpp"

class BlockChest : public BlockContainer
{
public:
    BlockChest(uint16_t id);

    uint8_t texture_index(World *world, const Vec3i &pos, uint8_t face) override;
    bool has_neighbor_chest(World *world, const Vec3i &pos);
    bool can_place(World *world, const Vec3i &pos) override;
    void on_removed(World *world, const Vec3i &pos) override;
    bool on_use(EntityPhysical *entity, const Vec3i &pos) override;

    TileEntity *get_tile_entity(World *world, const Vec3i &pos) override;
};
#pragma once

#include <block/block_base.hpp>

class BlockLadder : public BlockBase
{
public:
    BlockLadder(uint16_t id, uint8_t texture_index);

    virtual void get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual void on_placed(World *world, const Vec3i &pos, uint8_t face);
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
};
#pragma once

#include <block/block_base.hpp>

class BlockFence : public BlockBase
{
public:
    BlockFence(uint16_t id, uint8_t texture_index);
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual void get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;
    virtual bool is_opaque() override;
};
#pragma once

#include <block/block_base.hpp>

class BlockTorch : public BlockBase
{
public:
    BlockTorch(uint16_t id, uint8_t texture_index);
    virtual bool is_opaque() override;
    virtual void get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;
    virtual void get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;
    
    virtual bool can_stay(World *world, const Vec3i &pos) override;
    virtual void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual void on_placed(World *world, const Vec3i &pos, uint8_t face) override;
    virtual void on_added(World *world, const Vec3i &pos);
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;

};
#pragma once

#include <block/block_base.hpp>

class BlockButton : public BlockBase
{
public:
    BlockButton(uint16_t id, uint8_t texture_index);
    virtual bool is_opaque() override;
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual void get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;
    virtual void on_placed(World *world, const Vec3i &pos, uint8_t face);
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos);
    virtual void on_removed(World *world, const Vec3i &pos) override;
    virtual void on_click(EntityPhysical *entity, const Vec3i &pos) override;
    virtual bool provides_power(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool provides_indirect_power(World *world, const Vec3i &pos, uint8_t face) override;
    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random);
};
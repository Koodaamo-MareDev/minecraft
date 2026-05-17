#pragma once

#include <block/block_base.hpp>

class BlockRail : public BlockBase
{
public:
    BlockRail(uint16_t id, uint8_t texture_index);

    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    virtual bool is_opaque() override;
    virtual void get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;

protected:
    virtual void resolve_state(World *world, const Vec3i &pos);
};
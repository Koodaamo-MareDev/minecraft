#pragma once

#include <block/block_base.hpp>

class BlockCake : public BlockBase
{
public:
    BlockCake(uint16_t id, uint8_t texture_index);

    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    virtual bool is_opaque() override;
    virtual void get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list);
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos);
    virtual void on_click(EntityPhysical *entity, const Vec3i &pos);
    virtual bool can_stay(World *world, const Vec3i &pos) override;
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
    virtual uint16_t drop_count(javaport::Random &random) override;
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
};
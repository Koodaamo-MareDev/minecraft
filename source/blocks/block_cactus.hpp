#pragma once

#include <block/block_base.hpp>

class BlockCactus : public BlockBase
{
    
public:
    BlockCactus(uint16_t id, uint8_t texture_index);

    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual bool is_opaque() override;
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
    virtual bool can_stay(World *world, const Vec3i &pos) override;
    virtual void on_entity_collide(EntityPhysical *entity, const Vec3i &pos) override;
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
};
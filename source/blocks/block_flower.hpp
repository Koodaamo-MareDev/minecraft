#pragma once

#include <block/block_base.hpp>

class BlockFlower : public BlockBase
{

public:
    BlockFlower(uint16_t id, uint8_t texture_index, Materials material);
    BlockFlower(uint16_t id, uint8_t texture_index);
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual bool can_stay(World *world, const Vec3i &pos) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual bool is_opaque() override;

protected:
    virtual bool can_grow_on(uint8_t block_id);

    void check_can_stay(World *world, const Vec3i &pos);
};
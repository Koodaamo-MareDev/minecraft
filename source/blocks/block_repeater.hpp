#pragma once

#include <block/block_base.hpp>

class BlockRepeater : public BlockBase
{
public:
    BlockRepeater(uint16_t id, bool active);

    virtual bool is_opaque();
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual bool can_stay(World *world, const Vec3i &pos) override;
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    virtual bool should_render_side(World *world, const Vec3i &pos, uint8_t face) override;
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;

    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id) override;
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos) override;
    virtual void on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity) override;

    virtual bool provides_power(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool provides_indirect_power(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool is_power_source() override;

    bool gets_signal(World *world, const Vec3i &pos, uint8_t meta);

private:
    static int meta_to_ticks(uint8_t meta);
    bool active;
};
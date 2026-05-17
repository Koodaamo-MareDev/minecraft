#pragma once

#include <block/block_base.hpp>

class BlockFluids : public BlockBase
{
public:
    BlockFluids(uint16_t id, Materials material);
    virtual uint8_t texture_index(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool should_render_side(World *world, const Vec3i &pos, uint8_t face) override;
    virtual void apply_entity_velocity(EntityPhysical *entity, const Vec3i &pos) override;
    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
protected:
    virtual int tick_rate();
    int get_fluid_level_at(World *world, const Vec3i &pos);
    int get_min_fluid_level(World *world, const Vec3i &pos, int src_level, int &sources);
    void set_fluid_stationary(World *world, const Vec3i &pos);
    void harden(World* world, const Vec3i& pos);

    static float get_height(int level);
    static bool can_any_fluid_replace(BlockID id);
    static bool can_fluid_replace(BlockID fluid, BlockID id);
};
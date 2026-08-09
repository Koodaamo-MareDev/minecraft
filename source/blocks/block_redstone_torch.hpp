#pragma once

#include <world/world.hpp>
#include <blocks/block_torch.hpp>
#include <world/redstone_update.hpp>
#include <list>

class BlockRedstoneTorch : public BlockTorch
{
public:
    BlockRedstoneTorch(uint16_t id, uint8_t texture_index, bool active);

    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id) override;
    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_removed(World *world, const Vec3i &pos) override;
    virtual bool provides_power(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool provides_indirect_power(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool is_power_source() override;

    bool check_burnout(World *world, const Vec3i &pos, bool add);
    bool gets_signal(World *world, const Vec3i &pos);

private:
    int32_t delay_ticks = 2;
    std::list<RedstoneUpdate> updates;
    bool active;
};
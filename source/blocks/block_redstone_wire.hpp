#pragma once

#include <block/block_base.hpp>
#include <tuple>
#include <set>

class BlockRedstoneWire : public BlockBase
{
private:
    // FIXME: This hacky variable is ported as is. The reason it exists is
    // likely due to some oversight in the original redstone implementation.
    bool wires_provide_power = true;
    std::set<std::pair<World *, Vec3i>> updates;

public:
    BlockRedstoneWire(uint16_t id, uint8_t texture_index);

    virtual void get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;
    virtual bool is_opaque() override;
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_removed(World *world, const Vec3i &pos) override;

    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id) override;

    virtual bool provides_power(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool provides_indirect_power(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool is_power_source() override;

    virtual bool colored() override;

    void common_notify_neighbors(World *world, const Vec3i &pos);

    void update_and_propagate(World *world, const Vec3i &pos);
    void propagate(World *world, const Vec3i &to, const Vec3i &from);
    void notify_wire_neighbors(World *world, const Vec3i &pos);
    int max_current_strength(World *world, const Vec3i &pos, int other);

    static bool is_source_or_wire(World *world, const Vec3i &pos);
};
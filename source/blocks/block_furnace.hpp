#pragma once

#include <blocks/block_container.hpp>

class BlockFurnace : public BlockContainer
{
public:
    BlockFurnace(uint16_t id, Materials material);

    virtual uint8_t texture_index(World *world, const Vec3i &pos, uint8_t face) override;
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta);
    virtual void on_added(World *world, const Vec3i &pos) override;
    virtual void on_removed(World *world, const Vec3i &pos) override;
    virtual void on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity) override;
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos) override;
    virtual TileEntity *get_tile_entity(World *world, const Vec3i &pos) override;

    static void update_state(World *world, const Vec3i &pos, bool state);

protected:
    void set_default_direction(World *world, const Vec3i &pos);
};
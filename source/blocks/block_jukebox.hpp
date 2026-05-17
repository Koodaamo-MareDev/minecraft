#pragma once

#include <block/block_base.hpp>

class BlockJukebox : public BlockBase
{
public:
    BlockJukebox(uint16_t id, uint8_t texture_index);

    virtual uint8_t texture_index(World *world, const Vec3i &pos, uint8_t face) override;
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos) override;
    virtual void drop_item_with_chance(World *world, const Vec3i &pos, uint8_t meta, float chance) override;


    void eject(World* world, const Vec3i& pos);
};
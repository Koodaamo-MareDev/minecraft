#pragma once

#include <block/block_base.hpp>

class BlockPumpkin : public BlockBase
{
public:
    BlockPumpkin(uint16_t id, uint8_t texture_index, Materials material);

    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    virtual bool can_place(World *world, const Vec3i &pos) override;
    virtual void on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity) override;


};
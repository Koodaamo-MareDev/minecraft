#pragma once

#include <block/block_base.hpp>

class BlockWorkbench : public BlockBase
{
public:
    BlockWorkbench(int16_t id);

    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos) override;
};
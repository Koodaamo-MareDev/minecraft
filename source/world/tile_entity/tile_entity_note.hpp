#pragma once

#include "tile_entity.hpp"

class TileEntityNote : public TileEntity
{
public:
    int8_t note = 0;
    bool prev_redstone_state = false;

    TileEntityNote();
    NBTTagCompound *serialize() override;
    void deserialize(NBTTagCompound *nbt) override;
    std::string id() override;
    void trigger_note(World *world, const Vec3i &pos);
    void change_pitch();
};
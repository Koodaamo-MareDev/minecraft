#pragma once

#include "tile_entity.hpp"

class TileEntitySign : public TileEntity
{
public:
    TileEntitySign();
    NBTTagCompound *serialize() override;
    void deserialize(NBTTagCompound *nbt) override;
    std::string id() override;
    std::string lines[4];
};
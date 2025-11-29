#ifndef TILE_ENTITY_CHEST_HPP
#define TILE_ENTITY_CHEST_HPP

#include "tile_entity.hpp"
#include "item/inventory.hpp"

class TileEntityChest : public TileEntity
{
public:
    inventory::Container items = inventory::Container(27);

    NBTTagCompound *serialize() override;
    void deserialize(NBTTagCompound *nbt) override;

    std::string id() override;

    virtual ~TileEntityChest() = default;
};

#endif
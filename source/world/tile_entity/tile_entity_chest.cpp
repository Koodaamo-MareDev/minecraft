#include "tile_entity_chest.hpp"

NBTTagCompound *TileEntityChest::serialize()
{
    NBTTagCompound *nbt = TileEntity::serialize();
    nbt->setTag("Items", items.serialize());
    return nbt;
}

void TileEntityChest::deserialize(NBTTagCompound *nbt)
{
    NBTTagList *item_list = nbt->getList("Items");
    if (item_list)
        items.deserialize(item_list);
}

std::string TileEntityChest::id()
{
    return "Chest";
}

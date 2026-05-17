#include "tile_entity_sign.hpp"

TileEntitySign::TileEntitySign()
{
}

NBTTagCompound *TileEntitySign::serialize()
{
    NBTTagCompound *nbt = TileEntity::serialize();
    for (size_t i = 0; i < 4; i++)
        nbt->setTag("Text" + std::to_string(i + 1), new NBTTagString(lines[i]));
    return nbt;
}

void TileEntitySign::deserialize(NBTTagCompound *nbt)
{
    for (size_t i = 0; i < 4; i++)
    {
        lines[i] = nbt->getString("Text" + std::to_string(i + 1));
        if (lines[i].size() > 15)
            lines[i] = lines[i].substr(0, 15);
    }
}

std::string TileEntitySign::id()
{
    return "Sign";
}

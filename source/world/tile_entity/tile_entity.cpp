#include "tile_entity.hpp"
#include <util/debuglog.hpp>
#include <stdexcept>

std::unordered_map<std::string, std::function<TileEntity *()>> TileEntity::id_to_ctor;

NBTTagCompound *TileEntity::serialize()
{
    NBTTagCompound *comp = new NBTTagCompound;
    comp->setTag("x", new NBTTagInt(pos.x));
    comp->setTag("y", new NBTTagInt(pos.y));
    comp->setTag("z", new NBTTagInt(pos.z));
    comp->setTag("id", new NBTTagString(id()));
    return comp;
}

void TileEntity::remove()
{
    chunk->remove_tile_entity(this);
}

TileEntity *TileEntity::load(NBTTagCompound *nbt, Chunk *chunk)
{
    std::string key = nbt->getString("id");
    if (key.empty())
    {
        throw std::runtime_error("TileEntity has no id");
    }

    // Find corresponding ctor using the TileEntity id
    if (auto val = id_to_ctor.find(key); val != id_to_ctor.end())
    {
        Vec3i pos = Vec3i(nbt->getInt("x"), nbt->getInt("y"), nbt->getInt("z"));

        if ((pos.x >> 4) != chunk->x || (pos.z >> 4) != chunk->z)
        {
            throw std::runtime_error("TileEntity is in wrong chunk");
        }

        // Everything's good, create the TileEntity
        TileEntity *te = val->second();

        te->chunk = chunk;
        te->pos = pos;
        te->deserialize(nbt);

        return te;
    }
    else
    {
        throw std::runtime_error("Invalid TileEntity id " + key);
    }
}
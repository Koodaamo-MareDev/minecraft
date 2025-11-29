#ifndef TILE_ENTITY_HPP
#define TILE_ENTITY_HPP

#include <unordered_map>
#include <string>
#include <functional>

#include <world/chunk.hpp>

class TileEntity
{
public:
    Vec3i pos;
    Chunk *chunk;

    static TileEntity *load(NBTTagCompound *nbt, Chunk *chunk);

    virtual NBTTagCompound *serialize();
    virtual void deserialize(NBTTagCompound *nbt) = 0;

    virtual std::string id() = 0;

    virtual ~TileEntity() = default;
    
    virtual void remove();

    template <typename T>
    static void register_ctor(const std::string &name)
    {
        id_to_ctor[name] = [](void)
        { return new T(); };
    }

private:
    static std::unordered_map<std::string, std::function<TileEntity *()>> id_to_ctor;
};

#endif
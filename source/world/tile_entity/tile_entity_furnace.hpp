#ifndef TILE_ENTITY_FURNACE_HPP
#define TILE_ENTITY_FURNACE_HPP

#include "tile_entity.hpp"
#include <item/inventory.hpp>

class World;

class TileEntityFurnace : public TileEntity
{
public:
    inventory::Container items = inventory::Container(3);

    int item_burn_time = 0;
    int burn_time = 0;
    int cook_time = 0;

    NBTTagCompound *serialize() override;
    void deserialize(NBTTagCompound *nbt) override;

    bool can_smelt();

    void tick(World *world) override;

    static int get_burn_time(item::ItemStack &fuel);

    std::string id() override;

    virtual ~TileEntityFurnace();
};

#endif
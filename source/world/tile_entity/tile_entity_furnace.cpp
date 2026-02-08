#include "tile_entity_furnace.hpp"

#include <world/world.hpp>
#include <crafting/recipe_manager.hpp>
#include <block/block_properties.hpp>
#include <item/item_id.hpp>

NBTTagCompound *TileEntityFurnace::serialize()
{
    NBTTagCompound *nbt = TileEntity::serialize();
    nbt->setTag("Items", items.serialize());
    return nbt;
}

void TileEntityFurnace::deserialize(NBTTagCompound *nbt)
{
    NBTTagList *item_list = nbt->getList("Items");
    if (item_list)
        items.deserialize(item_list);
    burn_time = nbt->getShort("BurnTime");
    cook_time = nbt->getShort("CookTime");
}

bool TileEntityFurnace::can_smelt()
{
    item::ItemStack &input = items[0];
    item::ItemStack &output = items[2];
    if (input.empty())
        return false;
    item::ItemStack result = crafting::RecipeManager::instance().smelt(input);
    if (result.empty())
        return false;
    if (output.empty())
        return true;
    if (output.id != result.id || output.meta != result.meta)
        return false;
    if (output.count >= output.as_item().max_stack)
        return false;
    if (output.count >= 64)
        return false;
    return true;
}

int TileEntityFurnace::get_burn_time(item::ItemStack &fuel)
{
    if (fuel.empty())
        return 0;
    if (fuel.id < 256)
    {
        if (properties(fuel.id).m_material == Materials::WOOD)
            return 300;
    }
    else
    {
        if (fuel.id == +ItemID::stick)
            return 100;
        if (fuel.id == +ItemID::coal)
            return 1600;
        if (fuel.id == +ItemID::lava_bucket)
            return 20000;
    }
    return 0;
}

void TileEntityFurnace::tick(World *world)
{
    item::ItemStack &input = items[0];
    item::ItemStack &fuel = items[1];
    item::ItemStack &output = items[2];

    bool was_burning = burn_time > 0;
    bool updated = false;

    if (was_burning)
        burn_time--;
    if (!world->is_remote())
    {
        if (burn_time == 0 && can_smelt())
        {
            item_burn_time = burn_time = get_burn_time(fuel);
            if (burn_time > 0)
            {
                updated = true;
                if (!fuel.empty())
                {
                    fuel.count--;

                    // Auto-clear the stack if needed
                    fuel.empty();
                }
            }
        }
        bool is_burning = burn_time > 0;
        if (is_burning && can_smelt())
        {
            if (++cook_time == 200)
            {
                cook_time = 0;
                item::ItemStack result = crafting::RecipeManager::instance().smelt(input);
                if (output.empty())
                {
                    output = result;
                }
                else
                {
                    output.count++;
                }
                input.count--;

                // Auto-clear the stack if needed
                input.empty();

                updated = true;
            }
        }
        else
            cook_time = 0;
        if (is_burning != was_burning)
        {
            updated = true;
            world->set_block_at(pos, is_burning ? BlockID::lit_furnace : BlockID::furnace);
        }
    }
    if (updated)
    {
        // TODO: Update GUI
    }
}

std::string TileEntityFurnace::id()
{
    return "Furnace";
}

TileEntityFurnace::~TileEntityFurnace()
{
    // TODO: Remove any associated GUI instance.
}

#include "inventory.hpp"

namespace inventory
{
    Item item_list[2560];

    void init_items()
    {
        // Blocks
        for (size_t i = 0; i < 256; i++)
        {
            item_list[i] = Item(i);
        }

        // Items
        item_list[+ItemID::iron_shovel] = Item(+ItemID::iron_shovel, 82).set_tool_properties(tool_type::shovel, tool_tier::iron, 4);
        item_list[+ItemID::iron_pickaxe] = Item(+ItemID::iron_pickaxe, 98).set_tool_properties(tool_type::pickaxe, tool_tier::iron, 5);
        item_list[+ItemID::iron_axe] = Item(+ItemID::iron_axe, 114).set_tool_properties(tool_type::axe, tool_tier::iron, 6);
        item_list[+ItemID::flint_and_steel] = Item(+ItemID::flint_and_steel, 5);
        item_list[+ItemID::apple] = Item(+ItemID::apple, 10);
        item_list[+ItemID::bow] = Item(+ItemID::bow, 21);
        item_list[+ItemID::arrow] = Item(+ItemID::arrow, 37);
        item_list[+ItemID::coal] = Item(+ItemID::coal, 7);
        item_list[+ItemID::diamond] = Item(+ItemID::diamond, 55);
        item_list[+ItemID::iron] = Item(+ItemID::iron, 23);
        item_list[+ItemID::gold] = Item(+ItemID::gold, 39);
        item_list[+ItemID::iron_sword] = Item(+ItemID::iron_sword, 66).set_tool_properties(tool_type::sword, tool_tier::iron, 6);
        item_list[+ItemID::wooden_sword] = Item(+ItemID::wooden_sword, 64).set_tool_properties(tool_type::sword, tool_tier::wood, 4);
        item_list[+ItemID::wooden_shovel] = Item(+ItemID::wooden_shovel, 80).set_tool_properties(tool_type::shovel, tool_tier::wood, 1);
        item_list[+ItemID::wooden_pickaxe] = Item(+ItemID::wooden_pickaxe, 96).set_tool_properties(tool_type::pickaxe, tool_tier::wood, 2);
        item_list[+ItemID::wooden_axe] = Item(+ItemID::wooden_axe, 112).set_tool_properties(tool_type::axe, tool_tier::wood, 3);
        item_list[+ItemID::stone_sword] = Item(+ItemID::stone_sword, 65).set_tool_properties(tool_type::sword, tool_tier::stone, 5);
        item_list[+ItemID::stone_shovel] = Item(+ItemID::stone_shovel, 81).set_tool_properties(tool_type::shovel, tool_tier::stone, 2);
        item_list[+ItemID::stone_pickaxe] = Item(+ItemID::stone_pickaxe, 97).set_tool_properties(tool_type::pickaxe, tool_tier::stone, 3);
        item_list[+ItemID::stone_axe] = Item(+ItemID::stone_axe, 113).set_tool_properties(tool_type::axe, tool_tier::stone, 4);
        item_list[+ItemID::diamond_sword] = Item(+ItemID::diamond_sword, 67).set_tool_properties(tool_type::sword, tool_tier::diamond, 7);
        item_list[+ItemID::diamond_shovel] = Item(+ItemID::diamond_shovel, 83).set_tool_properties(tool_type::shovel, tool_tier::diamond, 4);
        item_list[+ItemID::diamond_pickaxe] = Item(+ItemID::diamond_pickaxe, 99).set_tool_properties(tool_type::pickaxe, tool_tier::diamond, 5);
        item_list[+ItemID::diamond_axe] = Item(+ItemID::diamond_axe, 115).set_tool_properties(tool_type::axe, tool_tier::diamond, 6);
        item_list[+ItemID::stick] = Item(+ItemID::stick, 53);
        item_list[+ItemID::bowl] = Item(+ItemID::bowl, 71);
        item_list[+ItemID::mushroom_stew] = Item(+ItemID::mushroom_stew, 72);
        item_list[+ItemID::gold_sword] = Item(+ItemID::gold_sword, 68).set_tool_properties(tool_type::sword, tool_tier::gold, 4);
        item_list[+ItemID::gold_shovel] = Item(+ItemID::gold_shovel, 84).set_tool_properties(tool_type::shovel, tool_tier::gold, 1);
        item_list[+ItemID::gold_pickaxe] = Item(+ItemID::gold_pickaxe, 100).set_tool_properties(tool_type::pickaxe, tool_tier::gold, 2);
        item_list[+ItemID::gold_axe] = Item(+ItemID::gold_axe, 116).set_tool_properties(tool_type::axe, tool_tier::gold, 3);
        item_list[+ItemID::string] = Item(+ItemID::string, 8);
        item_list[+ItemID::feather] = Item(+ItemID::feather, 24);
        item_list[+ItemID::gunpowder] = Item(+ItemID::gunpowder, 40);
        item_list[+ItemID::wooden_hoe] = Item(+ItemID::wooden_hoe, 128);
        item_list[+ItemID::stone_hoe] = Item(+ItemID::stone_hoe, 129);
        item_list[+ItemID::iron_hoe] = Item(+ItemID::iron_hoe, 130);
        item_list[+ItemID::diamond_hoe] = Item(+ItemID::diamond_hoe, 131);
        item_list[+ItemID::gold_hoe] = Item(+ItemID::gold_hoe, 132);
        item_list[+ItemID::seeds] = Item(+ItemID::seeds, 9);
        item_list[+ItemID::wheat] = Item(+ItemID::wheat, 25);
        item_list[+ItemID::bread] = Item(+ItemID::bread, 41);
        item_list[+ItemID::leather_cap] = Item(+ItemID::leather_cap, 0);
        item_list[+ItemID::leather_tunic] = Item(+ItemID::leather_tunic, 16);
        item_list[+ItemID::leather_pants] = Item(+ItemID::leather_pants, 32);
        item_list[+ItemID::leather_boots] = Item(+ItemID::leather_boots, 48);
        item_list[+ItemID::chainmail_helmet] = Item(+ItemID::chainmail_helmet, 1);
        item_list[+ItemID::chainmail_chestplate] = Item(+ItemID::chainmail_chestplate, 17);
        item_list[+ItemID::chainmail_leggings] = Item(+ItemID::chainmail_leggings, 33);
        item_list[+ItemID::chainmail_boots] = Item(+ItemID::chainmail_boots, 49);
        item_list[+ItemID::iron_helmet] = Item(+ItemID::iron_helmet, 2);
        item_list[+ItemID::iron_chestplate] = Item(+ItemID::iron_chestplate, 18);
        item_list[+ItemID::iron_leggings] = Item(+ItemID::iron_leggings, 34);
        item_list[+ItemID::iron_boots] = Item(+ItemID::iron_boots, 50);
        item_list[+ItemID::diamond_helmet] = Item(+ItemID::diamond_helmet, 3);
        item_list[+ItemID::diamond_chestplate] = Item(+ItemID::diamond_chestplate, 19);
        item_list[+ItemID::diamond_leggings] = Item(+ItemID::diamond_leggings, 35);
        item_list[+ItemID::diamond_boots] = Item(+ItemID::diamond_boots, 51);
        item_list[+ItemID::gold_helmet] = Item(+ItemID::gold_helmet, 4);
        item_list[+ItemID::gold_chestplate] = Item(+ItemID::gold_chestplate, 20);
        item_list[+ItemID::gold_leggings] = Item(+ItemID::gold_leggings, 36);
        item_list[+ItemID::gold_boots] = Item(+ItemID::gold_boots, 52);
        item_list[+ItemID::flint] = Item(+ItemID::flint, 6);
        item_list[+ItemID::porkchop] = Item(+ItemID::porkchop, 87);
        item_list[+ItemID::cooked_porkchop] = Item(+ItemID::cooked_porkchop, 88);
        item_list[+ItemID::painting] = Item(+ItemID::painting, 26);
        item_list[+ItemID::golden_apple] = Item(+ItemID::golden_apple, 11);
        item_list[+ItemID::sign] = Item(+ItemID::sign, 42);
        item_list[+ItemID::door] = Item(+ItemID::door, 43);
        item_list[+ItemID::bucket] = Item(+ItemID::bucket, 74);
        item_list[+ItemID::water_bucket] = Item(+ItemID::water_bucket, 75);
        item_list[+ItemID::lava_bucket] = Item(+ItemID::lava_bucket, 76);
        item_list[+ItemID::minecart] = Item(+ItemID::minecart, 151);
        item_list[+ItemID::saddle] = Item(+ItemID::saddle, 120);
        item_list[+ItemID::iron_door] = Item(+ItemID::iron_door, 44);
        item_list[+ItemID::redstone_dust] = Item(+ItemID::redstone_dust, 56);
        item_list[+ItemID::snowball] = Item(+ItemID::snowball, 14);
        item_list[+ItemID::boat] = Item(+ItemID::boat, 152);
        item_list[+ItemID::leather] = Item(+ItemID::leather, 119);
        item_list[+ItemID::milk_bucket] = Item(+ItemID::milk_bucket, 77);
        item_list[+ItemID::brick] = Item(+ItemID::brick, 22);
        item_list[+ItemID::clay_ball] = Item(+ItemID::clay_ball, 57);
        item_list[+ItemID::reeds] = Item(+ItemID::reeds, 27);
        item_list[+ItemID::paper] = Item(+ItemID::paper, 58);
        item_list[+ItemID::book] = Item(+ItemID::book, 59);
        item_list[+ItemID::slime_ball] = Item(+ItemID::slime_ball, 30);
        item_list[+ItemID::chest_minecart] = Item(+ItemID::chest_minecart, 167);
        item_list[+ItemID::furnace_minecart] = Item(+ItemID::furnace_minecart, 183);
        item_list[+ItemID::egg] = Item(+ItemID::egg, 12);
        item_list[+ItemID::compass] = Item(+ItemID::compass, 54);
        item_list[+ItemID::fishing_rod] = Item(+ItemID::fishing_rod, 69);
        item_list[+ItemID::clock] = Item(+ItemID::clock, 70);
        item_list[+ItemID::glowstone_dust] = Item(+ItemID::glowstone_dust, 73);
        item_list[+ItemID::fish] = Item(+ItemID::fish, 89);
        item_list[+ItemID::cooked_fish] = Item(+ItemID::cooked_fish, 90);
        item_list[+ItemID::dye] = Item(+ItemID::dye, 78);
        item_list[+ItemID::bone] = Item(+ItemID::bone, 28);
        item_list[+ItemID::sugar] = Item(+ItemID::sugar, 13);
        item_list[+ItemID::cake] = Item(+ItemID::cake, 29);
        item_list[+ItemID::bed] = Item(+ItemID::bed, 45);
        item_list[+ItemID::diode] = Item(+ItemID::diode, 86);
        item_list[+ItemID::record_13] = Item(+ItemID::record_13, 240);
        item_list[+ItemID::record_cat] = Item(+ItemID::record_cat, 241);
    }

    void default_on_use(Item &item, Vec3i pos, Vec3i face, EntityPhysical *entity)
    {
        // Do nothing
    }

    void Container::clear()
    {
        for (size_t i = 0; i < size(); i++)
        {
            stacks[i] = ItemStack();
        }
    }

    size_t Container::size()
    {
        return stacks.size();
    }

    size_t Container::count()
    {
        size_t count = 0;
        for (size_t i = 0; i < size(); i++)
        {
            if (!stacks[i].empty())
            {
                count++;
            }
        }
        return count;
    }

    ItemStack Container::add(ItemStack stack)
    {
        if (stack.empty())
            return stack;

        while (stack.count > 0)
        {
            int slot = find_free_slot_for(stack);

            if (slot == -1)
                break;

            stack = place(stack, slot);
        }

        // Return the remaining stack if there's no space
        return stack;
    }

    void Container::replace(ItemStack stack, size_t index)
    {
        if (index < size())
        {
            stacks[index] = stack;
        }
    }

    ItemStack Container::place(ItemStack stack, size_t index)
    {
        if (index < size())
        {
            ItemStack &orig = stacks[index];

            // If the stack is empty, just place the stack
            if (orig.id == 0)
            {
                orig = stack;
                return ItemStack();
            }

            // If the stack is the same, add the stack
            if (orig.id == stack.id && orig.meta == stack.meta)
            {
                orig.count += stack.count;

                // If the stack is over the max stack size, split the stack
                if (orig.count > orig.as_item().max_stack)
                {
                    stack.count = orig.count - orig.as_item().max_stack;
                    orig.count = orig.as_item().max_stack;

                    // Return the remaining stack
                    return stack;
                }

                // Return an empty stack
                return ItemStack();
            }

            // Swap the stacks
            ItemStack temp = orig;
            orig = stack;
            return temp;
        }

        // Don't place the stack if the index is out of bounds
        return stack;
    }

    int Container::find_free_slot_for(ItemStack stack)
    {
        // If the stack is empty, return -1
        if (stack.empty())
            return -1;

        // Search for stacks with the same id and meta
        for (size_t i = 0; i < size() && i < usable_slots; i++)
        {
            ItemStack &current = stacks[i];
            size_t max_stack = current.as_item().max_stack;

            // Skip full stacks
            if (current.count >= max_stack)
            {
                continue;
            }

            if (current.id == stack.id && current.meta == stack.meta)
            {
                return i;
            }
        }

        // Place the stack in the first empty slot
        for (size_t i = 0; i < size() && i < usable_slots; i++)
        {
            if (stacks[i].empty())
            {
                return i;
            }
        }

        // No free slot found
        return -1;
    }

    float Item::get_efficiency(BlockID block_id, tool_type block_tool_type, tool_tier block_tool_tier) const
    {
        // Swords have a fixed efficiency
        if (this->tool == tool_type::sword)
            return 1.5f;

        // Skip if the item is not a tool
        if (this->tier == tool_tier::no_tier || this->tool == tool_type::none)
            return 1.0f;

        // Apply the efficiency based on tier if the tool matches
        if (this->tool == block_tool_type && this->tier >= block_tool_tier)
        {
            switch (this->tier)
            {
            case tool_tier::wood:
                return 2.0f;
            case tool_tier::gold:
                return 12.0f;
            case tool_tier::stone:
                return 4.0f;
            case tool_tier::iron:
                return 6.0f;
            case tool_tier::diamond:
                return 8.0f;
            default:
                break;
            }
        }

        return 1.0f;
    }

    int PlayerInventory::find_free_slot_for(ItemStack stack)
    {
        // If the stack is empty, return -1
        if (stack.empty() || size() < 45)
            return -1;

        // Search hotbar for stacks with the same id and meta (starting at 36 as the hotbar starts there)
        for (size_t i = 36; i < 45; i++)
        {
            ItemStack &current = stacks[i];
            size_t max_stack = current.as_item().max_stack;

            // Skip full stacks
            if (current.count >= max_stack)
            {
                continue;
            }

            if (current.id == stack.id && current.meta == stack.meta)
            {
                return i;
            }
        }

        // Place the stack in the first empty inventory slot (starting at 36)
        for (size_t i = 36; i < 45; i++)
        {
            if (stacks[i].empty())
            {
                return i;
            }
        }

        // Search inventory for stacks with the same id and meta (starting at 9 as the first 9 slots are reserved for armor and crafting)
        for (size_t i = 9; i < 36; i++)
        {
            ItemStack &current = stacks[i];
            size_t max_stack = current.as_item().max_stack;

            // Skip full stacks
            if (current.count >= max_stack)
            {
                continue;
            }

            if (current.id == stack.id && current.meta == stack.meta)
            {
                return i;
            }
        }

        // Place the stack in the first empty inventory slot (starting at 9)
        for (size_t i = 9; i < 36; i++)
        {
            if (stacks[i].empty())
            {
                return i;
            }
        }

        // No free slot found
        return -1;
    }
} // namespace inventory
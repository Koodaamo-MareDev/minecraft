#include "items.hpp"

namespace registry
{
    void register_items()
    {
        using namespace item;

        // Blocks
        for (size_t i = 0; i < 256; i++)
        {
            item_list[i] = Item(i);
        }

        // Items
        item_list[+ItemID::iron_shovel] = Item(+ItemID::iron_shovel, 82).set_tool_properties(ToolType::shovel, ToolTier::iron, 4);
        item_list[+ItemID::iron_pickaxe] = Item(+ItemID::iron_pickaxe, 98).set_tool_properties(ToolType::pickaxe, ToolTier::iron, 5);
        item_list[+ItemID::iron_axe] = Item(+ItemID::iron_axe, 114).set_tool_properties(ToolType::axe, ToolTier::iron, 6);
        item_list[+ItemID::flint_and_steel] = Item(+ItemID::flint_and_steel, 5);
        item_list[+ItemID::apple] = Item(+ItemID::apple, 10);
        item_list[+ItemID::bow] = Item(+ItemID::bow, 21);
        item_list[+ItemID::arrow] = Item(+ItemID::arrow, 37);
        item_list[+ItemID::coal] = Item(+ItemID::coal, 7);
        item_list[+ItemID::diamond] = Item(+ItemID::diamond, 55);
        item_list[+ItemID::iron] = Item(+ItemID::iron, 23);
        item_list[+ItemID::gold] = Item(+ItemID::gold, 39);
        item_list[+ItemID::iron_sword] = Item(+ItemID::iron_sword, 66).set_tool_properties(ToolType::sword, ToolTier::iron, 6);
        item_list[+ItemID::wooden_sword] = Item(+ItemID::wooden_sword, 64).set_tool_properties(ToolType::sword, ToolTier::wood, 4);
        item_list[+ItemID::wooden_shovel] = Item(+ItemID::wooden_shovel, 80).set_tool_properties(ToolType::shovel, ToolTier::wood, 1);
        item_list[+ItemID::wooden_pickaxe] = Item(+ItemID::wooden_pickaxe, 96).set_tool_properties(ToolType::pickaxe, ToolTier::wood, 2);
        item_list[+ItemID::wooden_axe] = Item(+ItemID::wooden_axe, 112).set_tool_properties(ToolType::axe, ToolTier::wood, 3);
        item_list[+ItemID::stone_sword] = Item(+ItemID::stone_sword, 65).set_tool_properties(ToolType::sword, ToolTier::stone, 5);
        item_list[+ItemID::stone_shovel] = Item(+ItemID::stone_shovel, 81).set_tool_properties(ToolType::shovel, ToolTier::stone, 2);
        item_list[+ItemID::stone_pickaxe] = Item(+ItemID::stone_pickaxe, 97).set_tool_properties(ToolType::pickaxe, ToolTier::stone, 3);
        item_list[+ItemID::stone_axe] = Item(+ItemID::stone_axe, 113).set_tool_properties(ToolType::axe, ToolTier::stone, 4);
        item_list[+ItemID::diamond_sword] = Item(+ItemID::diamond_sword, 67).set_tool_properties(ToolType::sword, ToolTier::diamond, 7);
        item_list[+ItemID::diamond_shovel] = Item(+ItemID::diamond_shovel, 83).set_tool_properties(ToolType::shovel, ToolTier::diamond, 4);
        item_list[+ItemID::diamond_pickaxe] = Item(+ItemID::diamond_pickaxe, 99).set_tool_properties(ToolType::pickaxe, ToolTier::diamond, 5);
        item_list[+ItemID::diamond_axe] = Item(+ItemID::diamond_axe, 115).set_tool_properties(ToolType::axe, ToolTier::diamond, 6);
        item_list[+ItemID::stick] = Item(+ItemID::stick, 53);
        item_list[+ItemID::bowl] = Item(+ItemID::bowl, 71);
        item_list[+ItemID::mushroom_stew] = Item(+ItemID::mushroom_stew, 72);
        item_list[+ItemID::gold_sword] = Item(+ItemID::gold_sword, 68).set_tool_properties(ToolType::sword, ToolTier::gold, 4);
        item_list[+ItemID::gold_shovel] = Item(+ItemID::gold_shovel, 84).set_tool_properties(ToolType::shovel, ToolTier::gold, 1);
        item_list[+ItemID::gold_pickaxe] = Item(+ItemID::gold_pickaxe, 100).set_tool_properties(ToolType::pickaxe, ToolTier::gold, 2);
        item_list[+ItemID::gold_axe] = Item(+ItemID::gold_axe, 116).set_tool_properties(ToolType::axe, ToolTier::gold, 3);
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
} // namespace registry

#include "block_list.hpp"

#include <blocks/block_grass.hpp>
#include <blocks/block_log.hpp>
#include <blocks/block_sapling.hpp>
#include <blocks/block_stationary.hpp>
#include <blocks/block_flowing.hpp>
#include <blocks/block_falling.hpp>
#include <blocks/block_gravel.hpp>
#include <blocks/block_ore.hpp>
#include <blocks/block_leaves.hpp>
#include <blocks/block_glass.hpp>
#include <blocks/block_sandstone.hpp>
#include <blocks/block_wool.hpp>
#include <blocks/block_flower.hpp>
#include <blocks/block_bookshelf.hpp>
#include <blocks/block_spawner.hpp>
#include <blocks/block_chest.hpp>
#include <blocks/block_workbench.hpp>
#include <blocks/block_crops.hpp>
#include <blocks/block_soil.hpp>
#include <blocks/block_furnace.hpp>
#include <blocks/block_sign.hpp>
#include <blocks/block_door.hpp>
#include <blocks/block_button.hpp>
#include <blocks/block_ice.hpp>
#include <blocks/block_cactus.hpp>
#include <blocks/block_clay.hpp>
#include <blocks/block_reeds.hpp>
#include <blocks/block_jukebox.hpp>
#include <blocks/block_fence.hpp>
#include <blocks/block_pumpkin.hpp>
#include <blocks/block_glowstone.hpp>
#include <blocks/block_cake.hpp>

#include <world/world.hpp>
#include <algorithm>

BlockBase *block_list[256];

void init_blocks()
{
    std::fill(block_list, &block_list[256], nullptr);
    new BlockBase(BlockID::air, 0, Materials::AIR);
    (new BlockBase(BlockID::stone, 1, Materials::ROCK))->set_hardness(1.5f).set_resistance(10.0f).set_sound_type(BlockSoundType::stone);
    (new BlockGrass(BlockID::grass, 2, Materials::GROUND))->set_hardness(0.6f);
    (new BlockBase(BlockID::dirt, 2, Materials::GROUND))->set_hardness(0.5f).set_sound_type(BlockSoundType::dirt);
    (new BlockBase(BlockID::cobblestone, 16, Materials::ROCK))->set_hardness(2.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::planks, 4, Materials::WOOD))->set_hardness(2.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::wood);
    (new BlockSapling(BlockID::sapling, 15))->set_hardness(0.0f);
    (new BlockBase(BlockID::bedrock, 17, Materials::ROCK))->set_hardness(-1.0f).set_resistance(6000000.0f).set_sound_type(BlockSoundType::stone);
    (new BlockFlowing(BlockID::flowing_water, Materials::WATER))->set_hardness(100.0f).set_light_opacity(3);
    (new BlockStationary(BlockID::water, Materials::WATER))->set_hardness(100.0f).set_light_opacity(3);
    (new BlockFlowing(BlockID::flowing_lava, Materials::LAVA))->set_hardness(0.0f).set_light_luminance(15).set_light_opacity(255);
    (new BlockStationary(BlockID::lava, Materials::LAVA))->set_hardness(0.0f).set_light_luminance(15).set_light_opacity(255);
    (new BlockFalling(BlockID::sand, 18, Materials::SAND))->set_hardness(0.5f).set_sound_type(BlockSoundType::sand);
    (new BlockGravel(BlockID::gravel, 19, Materials::SAND))->set_hardness(0.6f).set_sound_type(BlockSoundType::dirt);
    (new BlockOre(BlockID::gold_ore, 32))->set_hardness(3.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::stone);
    (new BlockOre(BlockID::iron_ore, 33))->set_hardness(3.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::stone);
    (new BlockOre(BlockID::coal_ore, 34))->set_hardness(3.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::stone);
    (new BlockLog(BlockID::wood))->set_hardness(2.0f).set_sound_type(BlockSoundType::wood);
    (new BlockLeaves(BlockID::leaves, 186))->set_hardness(0.2f).set_light_opacity(1).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::sponge, 48, Materials::SPONGE))->set_hardness(0.6f).set_sound_type(BlockSoundType::grass);
    (new BlockGlass(BlockID::glass, 49, Materials::GLASS))->set_hardness(0.3f).set_sound_type(BlockSoundType::glass);
    (new BlockOre(BlockID::lapis_ore, 160))->set_hardness(3.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::lapis_block, 144, Materials::ROCK))->set_hardness(3.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::dispenser, 46, Materials::ROCK))->set_hardness(3.5f).set_sound_type(BlockSoundType::stone);
    (new BlockSandStone(BlockID::sandstone, 192, Materials::ROCK))->set_hardness(0.8f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::note_block, 74, Materials::WOOD))->set_hardness(0.8f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::bed, 134, Materials::CLOTH))->set_hardness(0.2f).set_sound_type(BlockSoundType::stone).set_aabb(AABB(Vec3f(0.0, 0.0, 0.0), Vec3f(1.0, 9.0 / 16.0, 1.0)));
    // Rails?
    (new BlockFlower(BlockID::tallgrass, 39, Materials::PLANTS))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);
    (new BlockFlower(BlockID::deadbush, 55, Materials::PLANTS))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);

    (new BlockWool(BlockID::wool, 64))->set_hardness(0.8f).set_sound_type(BlockSoundType::cloth);
    // Piston extension
    (new BlockFlower(BlockID::dandelion, 13))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);
    (new BlockFlower(BlockID::rose, 12))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);
    (new BlockFlower(BlockID::brown_mushroom, 29))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);
    (new BlockFlower(BlockID::red_mushroom, 28))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);
    (new BlockBase(BlockID::gold_block, 23, Materials::IRON))->set_hardness(3.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::metal);
    (new BlockBase(BlockID::iron_block, 22, Materials::IRON))->set_hardness(5.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::metal);
    (new BlockBase(BlockID::double_stone_slab, Materials::ROCK))->set_hardness(2.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::stone_slab, Materials::ROCK))->set_hardness(2.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::bricks, 7, Materials::ROCK))->set_hardness(2.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::tnt, 8, Materials::TNT))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);
    (new BlockBookshelf(BlockID::bookshelf, 35))->set_hardness(1.5f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::mossy_cobblestone, 36, Materials::ROCK))->set_hardness(2.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::obsidian, 37, Materials::ROCK))->set_hardness(10.0f).set_resistance(2000.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::torch, 80, Materials::WOOD))->set_hardness(0.0f).set_light_luminance(15).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::fire, 31, Materials::WOOD))->set_hardness(0.0f).set_sound_type(BlockSoundType::cloth);
    (new BlockSpawner(BlockID::mob_spawner, 65))->set_hardness(5.0f).set_sound_type(BlockSoundType::metal);
    (new BlockBase(BlockID::oak_stairs, 4, Materials::WOOD))->set_hardness(2.0f).set_sound_type(BlockSoundType::wood);
    (new BlockChest(BlockID::chest))->set_hardness(2.5f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::redstone_wire, Materials::CIRCUITS))->set_hardness(0.0f).set_sound_type(BlockSoundType::stone);
    (new BlockOre(BlockID::diamond_ore, 50))->set_hardness(3.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::diamond_block, 24, Materials::IRON))->set_hardness(5.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::metal);
    (new BlockWorkbench(BlockID::crafting_table))->set_hardness(2.5f).set_sound_type(BlockSoundType::wood);
    (new BlockCrops(BlockID::wheat, 88, Materials::PLANTS))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);
    (new BlockSoil(BlockID::farmland, 87))->set_hardness(0.6f).set_sound_type(BlockSoundType::dirt);
    (new BlockFurnace(BlockID::furnace, Materials::ROCK))->set_hardness(3.5f).set_sound_type(BlockSoundType::stone);
    (new BlockFurnace(BlockID::lit_furnace, Materials::ROCK))->set_hardness(3.5f).set_light_luminance(13).set_sound_type(BlockSoundType::stone);
    (new BlockSign(BlockID::standing_sign, 4, true))->set_hardness(1.0f).set_sound_type(BlockSoundType::wood);
    (new BlockDoor(BlockID::wooden_door, Materials::WOOD))->set_hardness(3.0f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::ladder, 83, Materials::WOOD))->set_hardness(0.4f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::rail, 128, Materials::CIRCUITS))->set_hardness(0.7f).set_sound_type(BlockSoundType::metal);
    (new BlockBase(BlockID::stone_stairs, 16, Materials::ROCK))->set_hardness(2.0f).set_sound_type(BlockSoundType::stone);
    (new BlockSign(BlockID::wall_sign, 4, false))->set_hardness(1.0f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::lever, 16, Materials::CIRCUITS))->set_hardness(0.4f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::stone_pressure_plate, 0, Materials::CIRCUITS))->set_hardness(0.5f).set_sound_type(BlockSoundType::stone);
    (new BlockDoor(BlockID::iron_door, Materials::IRON))->set_hardness(5.0f).set_sound_type(BlockSoundType::metal);
    (new BlockBase(BlockID::wooden_pressure_plate, 4, Materials::CIRCUITS))->set_hardness(0.5f).set_sound_type(BlockSoundType::wood);
    (new BlockOre(BlockID::redstone_ore, 51))->set_hardness(3.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::stone);
    (new BlockOre(BlockID::lit_redstone_ore, 52))->set_hardness(3.0f).set_resistance(5.0f).set_light_luminance(9).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::unlit_redstone_torch, Materials::CIRCUITS))->set_hardness(0.0f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::redstone_torch, Materials::CIRCUITS))->set_hardness(0.0f).set_light_luminance(7).set_sound_type(BlockSoundType::wood);
    (new BlockButton(BlockID::stone_button, 0))->set_hardness(0.5f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::snow_layer, 66, Materials::SNOW_LAYER))->set_hardness(0.1f).set_sound_type(BlockSoundType::cloth);
    (new BlockIce(BlockID::ice, 67, Materials::ICE))->set_hardness(0.5f).set_light_opacity(3).set_sound_type(BlockSoundType::glass);
    (new BlockBase(BlockID::snow_block, 66, Materials::SNOW))->set_hardness(0.2f).set_sound_type(BlockSoundType::cloth);
    (new BlockCactus(BlockID::cactus, 70))->set_hardness(0.4f).set_sound_type(BlockSoundType::cloth);
    (new BlockClay(BlockID::clay, 72))->set_hardness(0.6f).set_sound_type(BlockSoundType::dirt);
    (new BlockReeds(BlockID::reeds, 73))->set_hardness(0.0f).set_sound_type(BlockSoundType::grass);
    (new BlockJukebox(BlockID::jukebox, 76))->set_hardness(2.0f).set_resistance(10.0f).set_sound_type(BlockSoundType::stone);
    (new BlockFence(BlockID::fence, 4))->set_hardness(2.0f).set_resistance(5.0f).set_sound_type(BlockSoundType::wood);
    (new BlockPumpkin(BlockID::pumpkin, 102, Materials::PUMPKIN))->set_hardness(1.0f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::netherrack, 103, Materials::ROCK))->set_hardness(0.4f).set_sound_type(BlockSoundType::stone);
    (new BlockBase(BlockID::soul_sand, 104, Materials::SAND))->set_hardness(0.5f).set_sound_type(BlockSoundType::sand);
    (new BlockGlowstone(BlockID::glowstone, 105, Materials::GLASS))->set_hardness(0.3f).set_sound_type(BlockSoundType::glass);
    (new BlockBase(BlockID::portal, 14, Materials::GLASS))->set_hardness(-1.0f).set_light_luminance(11).set_sound_type(BlockSoundType::glass);
    (new BlockPumpkin(BlockID::lit_pumpkin, 102, Materials::PUMPKIN))->set_hardness(1.0f).set_sound_type(BlockSoundType::wood);
    (new BlockCake(BlockID::cake, 121))->set_hardness(0.5f).set_sound_type(BlockSoundType::cloth);
    (new BlockBase(BlockID::unpowered_repeater, Materials::CIRCUITS))->set_hardness(0.0f).set_sound_type(BlockSoundType::wood);
    (new BlockBase(BlockID::powered_repeater, Materials::CIRCUITS))->set_hardness(0.0f).set_light_luminance(9).set_sound_type(BlockSoundType::wood);
}

BlockBase *block_at(World *world, const Vec3i &pos)
{
    BlockBase *block = block_list[world->get_block_id_at(pos)];

    return block ? block : block_list[0];
}

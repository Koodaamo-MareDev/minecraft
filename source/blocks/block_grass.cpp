#include "block_grass.hpp"

#include <world/world.hpp>
#include <block/block_list.hpp>

BlockGrass::BlockGrass(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
}

uint8_t BlockGrass::texture_index(World *world, const Vec3i &pos, uint8_t face)
{
    // Check top
    if (face == +BlockFace::NY)
        return 2;
    if (face == +BlockFace::PY)
        return 203;

    // Check if it's snowy on its sides
    if (world->get_meta_at(pos) == 1)
        return 68;

    return 202;
}

void BlockGrass::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    if (world->is_remote())
        return;
    Block *block = world->get_block_at(pos + Vec3i{0, 1, 0});
    if (!block)
        return;
    uint8_t light_value = block->block_light;
    if (block->sky_light > light_value)
        light_value = block->sky_light;

    if (light_value < 4 && block_list[block->id]->material().can_grass)
    {
        if (random.nextInt(4) != 0)
            return;

        world->set_block_at(pos, BlockID::dirt);
    }
    else if (light_value >= 9)
    {
        int x = random.nextInt(3) - 1;
        int y = random.nextInt(5) - 3;
        int z = random.nextInt(3) - 1;
        block = world->get_block_at(pos + Vec3i{x, y, z});
        if (!block)
            return;
        light_value = block->block_light;
        if (block->sky_light > light_value)
            light_value = block->sky_light;

        if (block->id == BlockID::dirt && light_value >= 4 && block_list[block->id]->material().can_grass)
            world->set_block_at(pos, BlockID::grass);
    }
}

uint16_t BlockGrass::drop_id(uint16_t meta, javaport::Random &random)
{
    return +BlockID::dirt;
}

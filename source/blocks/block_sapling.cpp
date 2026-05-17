#include "block_sapling.hpp"

#include <world/world.hpp>
#include <world/chunkprovider.hpp>

BlockSapling::BlockSapling(uint16_t id, uint8_t texture_index) : BlockFlower(id, texture_index)
{
    data.aabb = AABB(Vec3f(0.1, 0.0, 0.1), Vec3f(0.9, 0.8, 0.9));
}

void BlockSapling::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    Block *block = world->get_block_at(pos);
    if (block && std::max({uint8_t(block->block_light), uint8_t(block->sky_light)}) >= 9 && random.nextInt(5) == 0)
    {
        if (block->meta < 15)
            block->meta++;
        else
            grow_tree(world, pos, random);
    }
}

void BlockSapling::grow_tree(World *world, const Vec3i &pos, javaport::Random &random)
{
    // TODO: Generate tree using world->chunk_provider
}

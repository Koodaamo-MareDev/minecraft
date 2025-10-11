#ifndef WORLD_GEN_LIQUIDS_HPP
#define WORLD_GEN_LIQUIDS_HPP

#include <block/block_id.hpp>
#include <ported/WorldGenerator.hpp>
namespace javaport
{
    class WorldGenLiquids : public WorldGenerator
    {
    public:
        BlockID liquid;
        WorldGenLiquids(World *world, BlockID liquid) : liquid(liquid) { this->world = world; }
        bool generate(Random &rng, Vec3i pos);
    };
}

#endif
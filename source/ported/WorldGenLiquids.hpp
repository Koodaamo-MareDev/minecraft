#ifndef WORLD_GEN_LIQUIDS_HPP
#define WORLD_GEN_LIQUIDS_HPP

#include "WorldGenerator.hpp"
#include "../block_id.hpp"
namespace javaport
{
    class WorldGenLiquids : public WorldGenerator
    {
    public:
        BlockID liquid;
        WorldGenLiquids(BlockID liquid) : liquid(liquid) {}
        bool generate(Random &rng, Vec3i pos);
    };
}

#endif
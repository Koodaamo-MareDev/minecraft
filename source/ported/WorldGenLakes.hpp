#ifndef WORLD_GEN_LAKES_HPP
#define WORLD_GEN_LAKES_HPP

#include <block/block_id.hpp>
#include <ported/WorldGenerator.hpp>
namespace javaport
{
    class WorldGenLakes : public WorldGenerator
    {
    public:
        BlockID liquid;
        WorldGenLakes(BlockID liquid) : liquid(liquid) {}
        bool generate(Random &rng, Vec3i pos);
    };
}

#endif
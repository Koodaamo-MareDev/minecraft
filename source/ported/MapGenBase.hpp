#ifndef MAPGENBASE_HPP
#define MAPGENBASE_HPP

#include <cstdint>
#include <ported/Random.hpp>
#include <ported/SystemTime.hpp>
#include <world/chunk.hpp>

namespace javaport
{
    class MapGenBase
    {
    public:
        MapGenBase() {}
        void generate(Chunk *chunk, int64_t seed, BlockID *out);
        virtual void populate(int32_t chunkX, int32_t chunkZ, int32_t x, int32_t z, BlockID *out) {}

    protected:
        int32_t maxDist = 8;
        Random rng;
    };
}

#endif
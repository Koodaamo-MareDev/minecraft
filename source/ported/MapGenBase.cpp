#include "MapGenBase.hpp"
namespace javaport
{

    void MapGenBase::generate(Chunk *chunk, int64_t seed, BlockID *out)
    {
        rng.setSeed(seed);
        int64_t x_rand = (rng.nextLong()) | 1;
        int64_t z_rand = (rng.nextLong()) | 1;
        for (int x = chunk->x - maxDist; x <= chunk->x + maxDist; x++)
            for (int z = chunk->z - maxDist; z <= chunk->z + maxDist; z++)
            {
                rng.setSeed(((int64_t)x_rand * x + (int64_t)z_rand * z) ^ seed);
                populate(x, z, chunk->x, chunk->z, out);
            }
    }
}
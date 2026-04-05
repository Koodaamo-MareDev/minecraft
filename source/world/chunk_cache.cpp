#include "chunk_cache.hpp"
#include "world.hpp"

ChunkCache build_chunk_cache(World *world, int cx, int cz)
{
    ChunkCache cache{};
    cache.base_cx = cx;
    cache.base_cz = cz;

    for (int dz = -1; dz <= 1; dz++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            cache.chunks[dx + 1][dz + 1] =
                world->get_chunk(cx + dx, cz + dz);
        }
    }
    return cache;
}

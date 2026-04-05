#include "chunk_cache.hpp"
#include <world/world.hpp>
#include <util/constants.hpp>
#include <world/chunk.hpp>

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

Block *get_block_cached(
    ChunkCache &cache,
    int x, int y, int z,
    Chunk *&out_chunk)
{
    if (y < 0 || y > MAX_WORLD_Y)
        return nullptr;

    int cx = x >> 4;
    int cz = z >> 4;

    int dx = cx - cache.base_cx;
    int dz = cz - cache.base_cz;

    if ((unsigned)(dx + 1) > 2 || (unsigned)(dz + 1) > 2)
        return nullptr;

    Chunk *chunk = cache.chunks[dx + 1][dz + 1];
    if (!chunk)
        return nullptr;

    out_chunk = chunk;
    return &chunk->blockstates[(y << 8) | ((z & 15) << 4) | (x & 15)];
}

void get_neighbors_cached(ChunkCache &cache, int x, int y, int z, Block **out_neighbors)
{
    Chunk *_out_chunk;
    for (int i = 0; i < 6; i++)
    {
        Vec3i offset = face_offsets[i];
        out_neighbors[i] = get_block_cached(cache, x + offset.x, y + offset.y, z + offset.z, _out_chunk);
    }
}

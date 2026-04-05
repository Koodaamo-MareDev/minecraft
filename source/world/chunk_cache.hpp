#ifndef CHUNK_CACHE_HPP
#define CHUNK_CACHE_HPP

class Chunk;
class World;

struct ChunkCache
{
    Chunk *chunks[3][3]; // [dx+1][dz+1]
    int base_cx;
    int base_cz;
};

ChunkCache build_chunk_cache(World *world, int cx, int cz);

#endif
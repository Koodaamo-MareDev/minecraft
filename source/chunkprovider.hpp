#ifndef CHUNK_PROVIDER_HPP
#define CHUNK_PROVIDER_HPP
#define WORLDGEN_TREE_ATTEMPTS 8

#include <cstdint>

#include "util/constants.hpp"
#include "block_id.hpp"
#include "ported/NoiseSynthesizer.hpp"

class Chunk;

class ChunkProvider
{
public:
    ChunkProvider() {};
    virtual ~ChunkProvider() {};

    virtual void provide_chunk(Chunk *chunk)
    {
    }

    virtual void populate_chunk(Chunk *chunk)
    {
    }

protected:
    NoiseSynthesizer noiser;
    BlockID blocks[16 * 16 * WORLD_HEIGHT];
};

class ChunkProviderOverworld : public ChunkProvider
{
public:
    ChunkProviderOverworld(uint64_t seed);

    virtual void provide_chunk(Chunk *chunk);
    virtual void populate_chunk(Chunk *chunk);

protected: 
    void plant_tree(Vec3i pos, int height);
    void generate_trees(Vec3i pos, javaport::Random &rng);
    
    void generate_flowers(Vec3i pos, javaport::Random &rng);
    
    void generate_vein(Vec3i pos, BlockID id, javaport::Random &rng);
    void generate_ore_type(Vec3i pos, BlockID id, int count, int max_height, javaport::Random &rng);
    void generate_ores(Vec3i pos, javaport::Random &rng);
    
    void generate_features(Chunk *chunk);
};

#endif
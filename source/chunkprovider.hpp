#ifndef CHUNK_PROVIDER_HPP
#define CHUNK_PROVIDER_HPP
#define WORLDGEN_TREE_ATTEMPTS 8

#include <cstdint>

#include "util/constants.hpp"
#include "block_id.hpp"
#include "ported/NoiseSynthesizer.hpp"

class chunk_t;

class chunkprovider
{
public:
    chunkprovider() {};
    virtual ~chunkprovider() {};

    virtual void provide_chunk(chunk_t *chunk)
    {
    }

    virtual void populate_chunk(chunk_t *chunk)
    {
    }

protected:
    NoiseSynthesizer noiser;
    BlockID blocks[16 * 16 * WORLD_HEIGHT];
};

class chunkprovider_overworld : public chunkprovider
{
public:
    chunkprovider_overworld(uint64_t seed);

    virtual void provide_chunk(chunk_t *chunk);
    virtual void populate_chunk(chunk_t *chunk);

protected: 
    void plant_tree(vec3i pos, int height, chunk_t *chunk);
    void generate_trees(vec3i pos, chunk_t *chunk, javaport::Random &rng);
    
    void generate_flowers(vec3i pos, chunk_t *chunk, javaport::Random &rng);
    
    void generate_vein(vec3i pos, BlockID id, chunk_t *chunk, javaport::Random &rng);
    void generate_ore_type(vec3i pos, BlockID id, int count, int max_height, chunk_t *chunk, javaport::Random &rng);
    void generate_ores(vec3i pos, chunk_t *chunk, javaport::Random &rng);
    
    void generate_features(chunk_t *chunk);
};

#endif
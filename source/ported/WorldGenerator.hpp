#ifndef WORLD_GENERATOR_HPP
#define WORLD_GENERATOR_HPP
#include "../chunk_new.hpp"
#include "JavaRandom.hpp"
namespace javaport
{
    class WorldGenerator
    {
    public:
        virtual bool generate(Random &rng, vec3i pos) = 0;
    };
}

#endif
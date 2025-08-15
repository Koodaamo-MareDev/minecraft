#ifndef WORLD_GENERATOR_HPP
#define WORLD_GENERATOR_HPP

#include <math/vec3i.hpp>

#include "Random.hpp"
namespace javaport
{
    class WorldGenerator
    {
    public:
        virtual bool generate(Random &rng, Vec3i pos) = 0;
    };
}

#endif
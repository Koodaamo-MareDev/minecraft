#ifndef MAPGENSURFACE_HPP
#define MAPGENSURFACE_HPP

#include "MapGenBase.hpp"
#include <cmath>

class NoiseSynthesizer;

namespace javaport
{
    class MapGenSurface : public MapGenBase
    {
    private:
        NoiseSynthesizer &noiser;

    public:
        MapGenSurface(NoiseSynthesizer &noiser) : noiser(noiser)
        {
            maxDist = 0;
        }

        void populate(int32_t offX, int32_t offZ, int32_t chunkX, int32_t chunkZ, BlockID *out_ids);
    };
}

#endif
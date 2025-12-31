#ifndef MAPGENTERRAIN_H
#define MAPGENTERRAIN_H

#include "MapGenBase.hpp"
#include <cmath>
#include <algorithm>

class NoiseSynthesizer;
namespace javaport
{
    class MapGenTerrain : public MapGenBase
    {
    private:
        NoiseSynthesizer &noiser;
    public:
        MapGenTerrain(NoiseSynthesizer &noiser) : noiser(noiser)
        {
            maxDist = 0;
        }
        void populate(int32_t offX, int32_t offZ, int32_t chunkX, int32_t chunkZ, BlockID *out_ids);
    };
}

#endif
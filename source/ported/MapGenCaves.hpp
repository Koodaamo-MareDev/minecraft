#ifndef MAPGENCAVES_H
#define MAPGENCAVES_H

#include "MapGenBase.hpp"
#include <cmath>
namespace javaport
{
    class MapGenCaves : public MapGenBase
    {
    public:
        void generateNode(int32_t offX, int32_t offZ, BlockID *out_ids, double x, double y, double z);

        void generateNode(int32_t chunkX, int32_t chunkZ, BlockID *out_ids, double x, double y, double z, float angleX, float angleY, float angleZ, int32_t offX, int32_t offZ, double radius);
        void populate(int32_t offX, int32_t offZ, int32_t chunkX, int32_t chunkZ, BlockID *out_ids);
    };
}
#endif
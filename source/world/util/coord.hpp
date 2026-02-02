#ifndef COORD_HPP
#define COORD_HPP

#include <math/vec3i.hpp>
#include <cstring>

class Chunk;

struct Coord
{
    union
    {
        struct
        {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            int8_t y;
            union
            {
                struct
                {
                    uint8_t z : 4;
                    uint8_t x : 4;
                };
                uint8_t h_index;
            };
#else
            union
            {
                struct
                {
                    uint8_t x : 4;
                    uint8_t z : 4;
                };
                uint8_t h_index;
            };
            int8_t y;
#endif
        } coords;
        uint16_t index;
        int16_t _index;
    };

    Chunk *chunk = nullptr;

    Coord(Vec3i pos, Chunk *chunk = nullptr) : chunk(chunk)
    {
        coords.x = pos.x & 0xF;
        coords.z = pos.z & 0xF;
        coords.y = pos.y & 0xFF;
    }

    int16_t s16() const
    {
        return _index;
    }

    uint16_t u16() const
    {
        return index;
    }

    Vec3i vec() const;
};

#endif
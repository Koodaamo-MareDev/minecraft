#ifndef COORD_HPP
#define COORD_HPP

#include <cstdint>
#include "math/vec3i.hpp"
#include <cstring>

class chunk_t;

struct coord
{
    union
    {
        struct
        {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            uint8_t y;
            uint8_t z : 4;
            uint8_t x : 4;
#else
            uint8_t x : 4;
            uint8_t z : 4;
            uint8_t y;
#endif
        } coords;
        uint16_t index;
        int16_t _index;
    };

    chunk_t *chunk = nullptr;

    coord(vec3i pos, chunk_t *chunk = nullptr) : chunk(chunk)
    {
        coords.x = pos.x & 0xF;
        coords.z = pos.z & 0xF;
        coords.y = pos.y & 0xFF;
    }

    coord(int16_t packed, chunk_t *chunk = nullptr) : _index(packed), chunk(chunk)
    {
    }

    coord(uint16_t packed, chunk_t *chunk = nullptr) : index(packed), chunk(chunk)
    {
    }

    operator int16_t() const
    {
        return _index;
    }

    operator uint16_t() const
    {
        return index;
    }

    operator vec3i() const;
};

#endif
#ifndef _BLOCK_HPP_
#define _BLOCK_HPP_

#include <stdint.h>
#include "block_id.hpp"

#define FACE_NX 0
#define FACE_PX 1
#define FACE_NY 2
#define FACE_PY 3
#define FACE_NZ 4
#define FACE_PZ 5
#define RENDER_FLAG_VISIBLE 6

class block_t
{
public:
    uint16_t id = 0;
    uint8_t light = 0;
    uint8_t visibility_flags = 0;
    uint32_t meta = 0;

    void set_opacity(uint8_t face, uint8_t flag);
    uint8_t get_cast_skylight();
    uint8_t get_cast_blocklight();

    BlockID get_blockid()
    {
        return BlockID(this->id);
    }

    void set_visibility(uint8_t flag)
    {
        this->set_opacity(6, flag);
    }

    uint8_t get_visibility()
    {
        return this->get_opacity(6);
    }

    uint8_t get_opacity(uint8_t face)
    {
        return (this->visibility_flags & (1 << face));
    }

    uint8_t get_skylight()
    {
        return (this->light >> 4) & 0xF;
    }

    uint8_t get_blocklight()
    {
        return this->light & 0xF;
    }

    void set_skylight(uint8_t value)
    {
        this->light = (this->light & 0xF) | (value << 4);
    }

    void set_blocklight(uint8_t value)
    {
        this->light = (this->light & 0xF0) | value;
    }

    void set_blockid(BlockID value)
    {
        this->id = uint16_t(value);
    }
};

#endif
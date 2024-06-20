#ifndef _BLOCK_HPP_
#define _BLOCK_HPP_

#include <stdint.h>
#include <cmath>
#include "block_id.hpp"
#include "blocks.hpp"
#include "aabb.hpp"
#include "vec3f.hpp"

#define FACE_NX 0
#define FACE_PX 1
#define FACE_NY 2
#define FACE_PY 3
#define FACE_NZ 4
#define FACE_PZ 5
#define RENDER_FLAG_VISIBLE 6

struct blockproperties_t
{
    BlockID id = BlockID::air;
    uint8_t default_state = 0;
    uint8_t texture_index = 0;
    uint8_t opacity = 0;
    uint8_t transparent = 0;
    uint8_t luminance = 0;
    uint8_t is_solid = 0;
    uint8_t is_fluid = 0;
    uint8_t fluid_decay = 0;
    BlockID base_fluid = BlockID::air;
    BlockID flow_fluid = BlockID::air;
};

class block_t
{
public:
    uint8_t id = 0;
    uint8_t visibility_flags = 0;
    uint8_t meta = 0;
    union
    {
        struct
        {
            uint8_t sky_light : 4;
            uint8_t block_light : 4;
        };

        uint8_t light;
    };

    BlockID get_blockid()
    {
        return BlockID(this->id);
    }

    void set_visibility(uint8_t flag)
    {
        this->visibility_flags = (this->visibility_flags & ~(0x40)) | (flag << 6);
    }

    uint8_t get_visibility()
    {
        return this->visibility_flags & (0x40);
    }

    uint8_t get_opacity(uint8_t face)
    {
        return (this->visibility_flags & (1 << face));
    }

    void set_opacity(uint8_t face, uint8_t flag)
    {
        this->visibility_flags &= ~(1 << face);
        this->visibility_flags |= (flag << face);
    }

    void set_blockid(BlockID value)
    {
        this->id = uint8_t(value);
        this->set_visibility(value != BlockID::air && !is_fluid(value));
    }

    int8_t get_cast_skylight()
    {
        int8_t opacity = get_block_opacity(get_blockid());
        int8_t cast_light = int8_t(sky_light) - std::max(opacity, int8_t(1));
        return std::max(cast_light, int8_t(0));
    }

    int8_t get_cast_blocklight()
    {
        int8_t opacity = get_block_opacity(get_blockid());
        int8_t cast_light = int8_t(block_light) - std::max(opacity, int8_t(1));
        return std::max(cast_light, int8_t(0));
    }

    aabb_t get_aabb(vec3i pos)
    {
        // TODO: Implement per-block AABBs
        return aabb_t(pos + vec3f(0, 0, 0), pos + vec3f(1, 1, 1));
    }
};

extern blockproperties_t block_properties[256];

#endif
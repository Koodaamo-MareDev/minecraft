#ifndef _BLOCK_HPP_
#define _BLOCK_HPP_

#include <stdint.h>
#include <cmath>
#include "block_id.hpp"
#include "aabb.hpp"
#include "vec3f.hpp"

#define FACE_NX 0
#define FACE_PX 1
#define FACE_NY 2
#define FACE_PY 3
#define FACE_NZ 4
#define FACE_PZ 5
#define RENDER_FLAG_VISIBLE 6

#define BLOCKPROP(A, B) block_properties[uint8_t(A)].B

enum class SoundType
{
    none,
    dirt,
    grass,
    sand,
    wood,
    stone,
    glass,
    metal,
    cloth,
};

struct blockproperties_t
{
    BlockID m_id = BlockID::air;
    uint8_t m_default_state = 0;
    uint8_t m_texture_index = 0;
    uint8_t m_opacity = 15;
    uint8_t m_transparent = 0;
    uint8_t m_luminance = 0;
    uint8_t m_solid = 1;
    uint8_t m_fluid = 0;
    uint8_t m_fluid_decay = 0;
    BlockID m_base_fluid = BlockID::air;
    BlockID m_flow_fluid = BlockID::air;
    SoundType m_sound_type = SoundType::none;
    uint8_t m_fall = 0;

    // FIXME: This is a temporary constructor replacement as the inlay hints are otherwise not visible.
    blockproperties_t &set(uint8_t default_state, uint8_t texture_index, uint8_t opacity, uint8_t transparent, uint8_t luminance, uint8_t is_solid, uint8_t is_fluid, uint8_t fluid_decay, BlockID base_fluid, BlockID flow_fluid, SoundType sound)
    {
        this->state(default_state);
        this->texture(texture_index);
        this->opacity(opacity);
        this->transparent(transparent);
        this->luminance(luminance);
        this->solid(is_solid);
        this->fluid(is_fluid);
        this->fluid_decay(fluid_decay);
        this->base_fluid(base_fluid);
        this->flow_fluid(flow_fluid);
        this->sound(sound);
        return *this;
    }

    // Have setters for each property
    blockproperties_t &id(BlockID value)
    {
        this->m_id = value;
        return *this;
    }

    blockproperties_t &state(uint8_t value)
    {
        this->m_default_state = value;
        return *this;
    }

    blockproperties_t &texture(uint8_t value)
    {
        this->m_texture_index = value;
        return *this;
    }

    blockproperties_t &opacity(uint8_t value)
    {
        this->m_opacity = value;
        return *this;
    }

    blockproperties_t &transparent(uint8_t value)
    {
        this->m_transparent = value;
        return *this;
    }

    blockproperties_t &luminance(uint8_t value)
    {
        this->m_luminance = value;
        return *this;
    }

    blockproperties_t &solid(bool value)
    {
        this->m_solid = value;
        return *this;
    }

    blockproperties_t &fluid(bool value)
    {
        this->m_fluid = value;
        return *this;
    }

    blockproperties_t &fluid_decay(uint8_t value) // Probably going to be removed
    {
        this->m_fluid_decay = value;
        return *this;
    }

    blockproperties_t &base_fluid(BlockID value)
    {
        this->m_base_fluid = value;
        return *this;
    }

    blockproperties_t &flow_fluid(BlockID value)
    {
        this->m_flow_fluid = value;
        return *this;
    }

    blockproperties_t &sound(SoundType value)
    {
        this->m_sound_type = value;
        return *this;
    }

    blockproperties_t &fall(bool value)
    {
        this->m_fall = value;
        return *this;
    }

    blockproperties_t() {}
};

extern blockproperties_t block_properties[256];

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
        this->set_visibility(value != BlockID::air && !BLOCKPROP(value, m_fluid));
    }

    int8_t get_cast_skylight()
    {
        int8_t opacity = BLOCKPROP(id, m_opacity);
        int8_t cast_light = int8_t(sky_light) - std::max(opacity, int8_t(1));
        return std::max(cast_light, int8_t(0));
    }

    int8_t get_cast_blocklight()
    {
        int8_t opacity = BLOCKPROP(id, m_opacity);
        int8_t cast_light = int8_t(block_light) - std::max(opacity, int8_t(1));
        return std::max(cast_light, int8_t(0));
    }

    aabb_t get_aabb(vec3i pos)
    {
        // TODO: Implement per-block AABBs
        return aabb_t(pos + vec3f(0, 0, 0), pos + vec3f(1, 1, 1));
    }
};

#endif
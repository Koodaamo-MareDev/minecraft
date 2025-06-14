#ifndef _BLOCK_HPP_
#define _BLOCK_HPP_

#include <stdint.h>
#include <cmath>
#include <vector>
#include <functional>
#include "block_id.hpp"
#include <math/aabb.hpp>
#include <math/vec3f.hpp>
#include <math/vec3i.hpp>
#include "inventory.hpp"

#define FACE_NX 0
#define FACE_PX 1
#define FACE_NY 2
#define FACE_PY 3
#define FACE_NZ 4
#define FACE_PZ 5
#define RENDER_FLAG_VISIBLE 6

enum class SoundType : uint8_t
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

enum class RenderType : uint8_t
{
    full,
    full_special,
    cross,
    flat_ground,
    slab,
    special,
};

enum class CollisionType : uint8_t
{
    none,
    solid,
    fluid,
    slab,

};
class block_t;

void default_aabb(const vec3i &pos, block_t *block, const aabb_t &other, std::vector<aabb_t> &aabb_list);
void slab_aabb(const vec3i &pos, block_t *block, const aabb_t &other, std::vector<aabb_t> &aabb_list);
void default_destroy(const vec3i &pos, const block_t &old_block);
inventory::item_stack default_drop(const block_t &old_block);

struct blockproperties_t
{
    BlockID m_id = BlockID::air;
    uint8_t m_default_state = 0;
    uint8_t m_texture_index = 0;
    uint8_t m_opacity = 15;
    uint8_t m_luminance = 0;
    uint8_t m_fluid_decay = 0;
    BlockID m_base_fluid = BlockID::air;
    BlockID m_flow_fluid = BlockID::air;
    SoundType m_sound_type = SoundType::none;
    RenderType m_render_type = RenderType::full;
    CollisionType m_collision = CollisionType::solid;
    float_t m_slipperiness = 0.6f;
    float_t m_blast_resistance = 0.5f;
    inventory::tool_type m_tool_type = inventory::tool_type::none;
    inventory::tool_tier m_tool_tier = inventory::tool_tier::no_tier;
    float_t m_hardness = 2.0f;

    union
    {
        struct
        {
            uint8_t m_fall : 1;
            uint8_t m_transparent : 1;
            uint8_t m_solid : 1;
            uint8_t m_fluid : 1;
            uint8_t m_valid_item : 1;
            uint8_t m_needs_support : 1;
        };
        uint8_t m_flags;
    };

    std::function<void(const vec3i &, block_t *, const aabb_t &, std::vector<aabb_t> &)> m_aabb;

    std::function<void(const vec3i &, const block_t &)> m_destroy;

    std::function<inventory::item_stack(const block_t &)> m_drops;

    bool intersects(const vec3i &pos, block_t *block, const aabb_t &other)
    {
        std::vector<aabb_t> aabb_list;
        m_aabb(pos, block, other, aabb_list);
        return aabb_list.size() > 0;
    }

    blockproperties_t()
    {
        // Set bit fields to default values
        m_fall = 0;
        m_transparent = 0;
        m_solid = 1;
        m_fluid = 0;
        m_valid_item = 1;
        m_needs_support = 0;
        m_aabb = default_aabb;
        m_destroy = default_destroy;
        m_drops = default_drop;
    }

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

    blockproperties_t &render_type(RenderType value)
    {
        this->m_render_type = value;
        return *this;
    }

    blockproperties_t &valid_item(bool value)
    {
        this->m_valid_item = value;
        return *this;
    }

    blockproperties_t &needs_support(bool value)
    {
        this->m_needs_support = value;
        return *this;
    }

    blockproperties_t &collision(CollisionType value)
    {
        this->m_collision = value;
        return *this;
    }

    blockproperties_t &slipperiness(float_t value)
    {
        this->m_slipperiness = value;
        return *this;
    }

    blockproperties_t &blast_resistance(float_t value)
    {
        this->m_blast_resistance = value;
        return *this;
    }

    blockproperties_t &aabb(std::function<void(const vec3i &, block_t *, const aabb_t &, std::vector<aabb_t> &)> aabb_func)
    {
        this->m_aabb = aabb_func;
        return *this;
    }

    blockproperties_t &destroy(std::function<void(const vec3i &, const block_t &)> destroy_func)
    {
        this->m_destroy = destroy_func;
        return *this;
    }

    blockproperties_t &drops(std::function<inventory::item_stack(const block_t &)> drops_func)
    {
        this->m_drops = drops_func;
        return *this;
    }

    blockproperties_t &tool(inventory::tool_type tool, inventory::tool_tier tier)
    {
        this->m_tool_type = tool;
        this->m_tool_tier = tier;
        return *this;
    }

    blockproperties_t &hardness(float value)
    {
        this->m_hardness = value;
        return *this;
    }

    bool can_be_broken_with(const inventory::item_stack &item) const
    {
        if (this->m_tool_type == inventory::tool_type::none)
            return true; // Can break with hands or any item

        if (this->m_tool_tier == inventory::tool_tier::no_tier)
            return true; // Can break with any item or hands, but still slow

        if (item.as_item().tool == this->m_tool_type && item.as_item().tier >= this->m_tool_tier)
            return true; // Can break with the correct tool and tier

        return false; // Cannot break with this item
    }

    float get_break_multiplier(const inventory::item_stack &item, bool on_ground, bool in_water) const
    {
        if (m_hardness < 0.0f)
            return 0.0f; // Block cannot be broken

        if (!this->can_be_broken_with(item))
            return 0.01f / this->m_hardness; // Cannot break with this item, use default speed

        if (item.as_item().tool == inventory::tool_type::none || (item.as_item().tool == this->m_tool_type && item.as_item().tier >= this->m_tool_tier))
            return item.as_item().get_efficiency(m_id, this->m_tool_type, this->m_tool_tier) * (on_ground ? 1.0f : 0.2f) * (in_water ? 0.2f : 1.0f) / this->m_hardness / 30.0f;

        return 1.0f; // Instant break with hands or any item
    }
};

extern blockproperties_t block_properties[256];

inline blockproperties_t &properties(BlockID id)
{
    return block_properties[uint8_t(id)];
}

inline blockproperties_t &properties(uint8_t id)
{
    return block_properties[id];
}

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
        this->set_visibility(value != BlockID::air && !properties(value).m_fluid);
    }

    int8_t get_cast_skylight()
    {
        uint8_t opacity = properties(id).m_opacity;
        return sky_light <= opacity ? 0 : sky_light - opacity;
    }

    int8_t get_cast_blocklight()
    {
        uint8_t opacity = properties(id).m_opacity;
        return block_light <= opacity ? 0 : block_light - opacity;
    }

    void get_aabb(const vec3i &pos, const aabb_t &other, std::vector<aabb_t> &aabb_list)
    {
        properties(id).m_aabb(pos, this, other, aabb_list);
    }

    bool intersects(const aabb_t &other, const vec3i &pos)
    {
        return properties(id).intersects(pos, this, other);
    }
};

#endif
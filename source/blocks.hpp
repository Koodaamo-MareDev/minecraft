#ifndef _BLOCKS_HPP_
#define _BLOCKS_HPP_

#include <cstddef>
#include "block.hpp"
#include "block_id.hpp"
#include "vec3i.hpp"

#define FLUID_UPDATE_REQUIRED_FLAG 0x10
#define FLUID_UPDATE_LATER_FLAG 0x20
#define FLOAT_TO_FLUIDMETA(A) (int(roundf((A)*8)))

int get_block_opacity(BlockID blockid);

bool is_face_transparent(int texture_index);

uint32_t get_default_texture_index(BlockID blockid);
uint32_t get_face_texture_index(block_t *block, int face);

bool is_solid(BlockID block_id);
void update_fluid(block_t *block, vec3i pos);
void set_fluid_level(block_t *block, uint8_t level);
uint8_t get_fluid_meta_level(block_t *block);
uint8_t get_fluid_visual_level(vec3i pos, BlockID block_id);

bool is_fluid_overridable(BlockID id);
float get_percent_air(int fluid_level);
float get_fluid_height(vec3i pos, BlockID block_type);

inline bool is_flowing_fluid(BlockID id)
{
    return id == BlockID::flowing_water || id == BlockID::flowing_lava;
}

inline bool is_still_fluid(BlockID id)
{
    return id == BlockID::water || id == BlockID::lava;
}

inline bool is_fluid(BlockID id)
{
    return id == BlockID::water || id == BlockID::flowing_water || id == BlockID::lava || id == BlockID::flowing_lava;
}

inline BlockID basefluid(BlockID id)
{
    if (id == BlockID::flowing_water || id == BlockID::water)
        return BlockID::water;
    if (id == BlockID::flowing_lava || id == BlockID::lava)
        return BlockID::lava;
    return BlockID::air;
}
inline BlockID flowfluid(BlockID id)
{
    if (id == BlockID::flowing_water || id == BlockID::water)
        return BlockID::flowing_water;
    if (id == BlockID::flowing_lava || id == BlockID::lava)
        return BlockID::flowing_lava;
    return BlockID::air;
}

inline bool is_same_fluid(BlockID id, BlockID other)
{
    return is_fluid(id) && is_fluid(other) && basefluid(id) == basefluid(other);
}

#endif
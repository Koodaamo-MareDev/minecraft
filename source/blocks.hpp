#ifndef _BLOCKS_HPP_
#define _BLOCKS_HPP_

#include <cstddef>
#include <math/vec3i.hpp>

#include "block.hpp"
#include "block_id.hpp"
#include "sounds.hpp"
#include "sound.hpp"

#define FLUID_UPDATE_REQUIRED_FLAG 0x10
#define FLOAT_TO_FLUIDMETA(A) (int(roundf((A)*8)))
class block_t;
class chunk_t;

int8_t get_block_opacity(BlockID blockid);

uint8_t get_block_luminance(BlockID block_id);

uint32_t get_default_texture_index(BlockID blockid);
uint32_t get_face_texture_index(block_t *block, int face);

bool is_solid(BlockID block_id);
void update_fluid(block_t *block, vec3i pos, chunk_t *near = nullptr);
void set_fluid_level(block_t *block, uint8_t level);
uint8_t get_fluid_meta_level(block_t *block);
uint8_t get_fluid_visual_level(vec3i pos, BlockID block_id, chunk_t *near = nullptr);

bool is_fluid_overridable(BlockID id);
float get_percent_air(int fluid_level);
float get_fluid_height(vec3i pos, BlockID block_type, chunk_t *near = nullptr);

sound get_step_sound(BlockID block_id);
sound get_mine_sound(BlockID block_id);
sound get_break_sound(BlockID block_id);

inline bool is_flowing_fluid(BlockID id)
{
    return id != BlockID::air && block_properties[static_cast<uint8_t>(id)].m_flow_fluid == id;
}

inline bool is_still_fluid(BlockID id)
{
    return id != BlockID::air && block_properties[static_cast<uint8_t>(id)].m_base_fluid == id;
}

inline bool is_fluid(BlockID id)
{
    return block_properties[static_cast<uint8_t>(id)].m_fluid;
}

// NOTE: This function assumes that the block is a fluid.
// Call is_fluid before calling this function.
inline BlockID basefluid(BlockID id)
{
    return block_properties[static_cast<uint8_t>(id)].m_base_fluid;
}

// NOTE: This function assumes that the block is a fluid.
// Call is_fluid before calling this function.
inline BlockID flowfluid(BlockID id)
{
    return block_properties[static_cast<uint8_t>(id)].m_flow_fluid;
}

inline bool is_same_fluid(BlockID id, BlockID other)
{
    return is_fluid(id) && basefluid(id) == basefluid(other);
}
#endif
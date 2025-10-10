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
class Block;
class Chunk;

int8_t get_block_opacity(BlockID blockid);

uint8_t get_block_luminance(BlockID block_id);

void override_texture_index(int32_t texture_index);
uint32_t get_default_texture_index(BlockID blockid);
uint32_t get_face_texture_index(Block *block, int face);

void schedule_update(const Vec3i &pos, Block &block);
bool is_solid(BlockID block_id);
int get_fluid_level_at(Vec3i pos, BlockID src_id);
int get_min_fluid_level(Vec3i pos, BlockID src_id, int src_fluid_level, int &sources);
void set_fluid_stationary(Vec3i pos);
void flow_fluid_later(Vec3i pos);
int get_flow_weight(Vec3i pos, BlockID block_id, int accumulated_weight, int previous_direction);
void get_flow_directions(Vec3i pos, BlockID block_id, bool directions[4]);
void stationary_fluid_tick(const Vec3i &pos, Block &block);
void flowing_fluid_tick(const Vec3i &pos, Block &block);
void stationary_fluid_neighbor_update(const Vec3i &pos, Block &block);
uint8_t get_fluid_meta_level(Block *block);
uint8_t get_fluid_visual_level(Vec3i pos, BlockID block_id);

bool can_any_fluid_replace(BlockID id);
bool can_fluid_replace(BlockID fluid, BlockID id);
float get_percent_air(int fluid_level);
float get_fluid_height(Vec3i pos, BlockID block_type);

Sound get_step_sound(BlockID block_id);
Sound get_mine_sound(BlockID block_id);
Sound get_break_sound(BlockID block_id);

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
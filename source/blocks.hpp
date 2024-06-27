#ifndef _BLOCKS_HPP_
#define _BLOCKS_HPP_

#include <cstddef>
#include "block_id.hpp"
#include "vec3i.hpp"

#define FLUID_UPDATE_REQUIRED_FLAG 0x10
#define FLOAT_TO_FLUIDMETA(A) (int(roundf((A)*8)))
class block_t;
class chunk_t;

int8_t get_block_opacity(BlockID blockid);

bool is_face_transparent(uint8_t texture_index);

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

// NOTE: This function assumes that the block is a fluid.
// Call is_fluid before calling this function.
inline BlockID basefluid(BlockID id)
{
    // We know that all fluids have bit 0x8 set
    // We also know that lava and flowing lava have bit 0x2 set
    // Bit 0x1 determines if the fluid is flowing or not - 0 for flowing, 1 for still
    // So we can just mask the bits to get the base fluid
    // This reduces the number of branches

    return static_cast<BlockID>((static_cast<uint8_t>(id) & 0x0A) | 0x01);
}

// NOTE: This function assumes that the block is a fluid.
// Call is_fluid before calling this function.
inline BlockID flowfluid(BlockID id)
{
    // Same as basefluid, but we clear the bit 0x1
    return static_cast<BlockID>(static_cast<uint8_t>(id) & 0x0A);
}

inline bool is_same_fluid(BlockID id, BlockID other)
{
    return is_fluid(id) && is_fluid(other) && basefluid(id) == basefluid(other);
}

#endif
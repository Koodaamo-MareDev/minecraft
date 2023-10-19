#ifndef _BLOCKS_H_
#define _BLOCKS_H_

#include <cstddef>
#include "block.hpp"
#include "block_id.hpp"
#include "vec3i.hpp"

#define FLUID_UPDATE_REQUIRED_FLAG 0x10
#define FLOAT_TO_FLUIDMETA(A) (int(roundf((A) * 8)))

int get_block_opacity(BlockID blockid);

bool is_face_transparent(int texture_index);

int get_face_texture_index(block_t * block, int face);

bool update_fluid(block_t *block, vec3i pos);
void set_fluid_level(block_t *block, uint8_t level);
uint8_t get_fluid_meta_level(block_t *block);
uint8_t get_fluid_visual_level(vec3i pos, BlockID block_id);
void set_fluid_full(block_t *block, uint8_t full);
uint8_t is_fluid_full(block_t *block);
bool is_still_fluid(BlockID id);
bool is_flowing_fluid(BlockID id);
bool is_fluid(BlockID id);
bool is_fluid_overridable(BlockID id);
BlockID basefluid(BlockID id);
bool is_same_fluid(BlockID id, BlockID other);
float get_percent_air(int fluid_level);
float get_fluid_height(vec3i pos, BlockID block_type);

#endif
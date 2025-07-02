#ifndef RENDER_BLOCKS_HPP
#define RENDER_BLOCKS_HPP

#include <cstdint>
#include <math/vec3i.hpp>

class chunk_t;
class block_t;

int render_section_blocks(chunk_t &chunk, int index, bool transparent, int vertexCount);
int render_block(chunk_t &chunk, block_t *block, const vec3i &pos, bool transparent);
int render_cube(chunk_t &chunk, block_t *block, const vec3i &pos, bool transparent);
int render_cube_special(chunk_t &chunk, block_t *block, const vec3i &pos, bool transparent);
int render_special(chunk_t &chunk, block_t *block, const vec3i &pos);
int render_flat_ground(chunk_t &chunk, block_t *block, const vec3i &pos);
int render_snow_layer(chunk_t &chunk, block_t *block, const vec3i &pos);
int render_chest(chunk_t &chunk, block_t *block, const vec3i &pos);
int render_torch(chunk_t &chunk, block_t *block, const vec3i &pos);
int render_torch_with_angle(chunk_t &chunk, block_t *block, const vec3f &vertex_pos, float ax, float az);
int render_door(chunk_t &chunk, block_t *block, const vec3i &pos);
int render_cross(chunk_t &chunk, block_t *block, const vec3i &pos);
int render_slab(chunk_t &chunk, block_t *block, const vec3i &pos);

int get_chest_texture_index(chunk_t &chunk, block_t *block, const vec3i &pos, uint8_t face);

#endif
#ifndef RENDER_BLOCKS_HPP
#define RENDER_BLOCKS_HPP

#include <cstdint>
#include <math/vec3i.hpp>

class Chunk;
class Block;

int render_section_blocks(Chunk &chunk, int index, bool transparent, int vertexCount);
int render_block(Block *block, const Vec3i &pos, bool transparent);
int render_cube(Block *block, const Vec3i &pos, bool transparent);
int render_inverted_cube(Block *block, const Vec3i &pos, bool transparent);
int render_cube_special(Block *block, const Vec3i &pos, bool transparent);
int render_inverted_cube_special(Block *block, const Vec3i &pos, bool transparent);
int render_special(Block *block, const Vec3i &pos);
int render_flat_ground(Block *block, const Vec3i &pos);
int render_snow_layer(Block *block, const Vec3i &pos);
int render_chest(Block *block, const Vec3i &pos);
int render_torch(Block *block, const Vec3i &pos);
int render_torch_with_angle(Block *block, const Vec3f &vertex_pos, float ax, float az);
int render_cactus(Block *block, const Vec3i &pos);
int render_leaves(Block *block, const Vec3i &pos);
int render_door(Block *block, const Vec3i &pos);
int render_cross(Block *block, const Vec3i &pos);
int render_slab(Block *block, const Vec3i &pos);

int get_chest_texture_index(Block *block, const Vec3i &pos, uint8_t face);

#endif
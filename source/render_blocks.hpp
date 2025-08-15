#ifndef RENDER_BLOCKS_HPP
#define RENDER_BLOCKS_HPP

#include <cstdint>
#include <math/vec3i.hpp>

class Chunk;
class Block;

int render_section_blocks(Chunk &chunk, int index, bool transparent, int vertexCount);
int render_block(Chunk &chunk, Block *block, const Vec3i &pos, bool transparent);
int render_cube(Chunk &chunk, Block *block, const Vec3i &pos, bool transparent);
int render_inverted_cube(Chunk &chunk, Block *block, const Vec3i &pos, bool transparent);
int render_cube_special(Chunk &chunk, Block *block, const Vec3i &pos, bool transparent);
int render_inverted_cube_special(Chunk &chunk, Block *block, const Vec3i &pos, bool transparent);
int render_special(Chunk &chunk, Block *block, const Vec3i &pos);
int render_flat_ground(Chunk &chunk, Block *block, const Vec3i &pos);
int render_snow_layer(Chunk &chunk, Block *block, const Vec3i &pos);
int render_chest(Chunk &chunk, Block *block, const Vec3i &pos);
int render_torch(Chunk &chunk, Block *block, const Vec3i &pos);
int render_torch_with_angle(Chunk &chunk, Block *block, const Vec3f &vertex_pos, float ax, float az);
int render_cactus(Chunk &chunk, Block *block, const Vec3i &pos);
int render_leaves(Chunk &chunk, Block *block, const Vec3i &pos);
int render_door(Chunk &chunk, Block *block, const Vec3i &pos);
int render_cross(Chunk &chunk, Block *block, const Vec3i &pos);
int render_slab(Chunk &chunk, Block *block, const Vec3i &pos);

int get_chest_texture_index(Chunk &chunk, Block *block, const Vec3i &pos, uint8_t face);

#endif
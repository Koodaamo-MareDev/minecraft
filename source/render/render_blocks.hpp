#ifndef RENDER_BLOCKS_HPP
#define RENDER_BLOCKS_HPP

#include <cstdint>
#include <math/vec3i.hpp>
#include <gertex/displaylist.hpp>

class Block;

int render_block(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_cube(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_inverted_cube(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_cube_special(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_inverted_cube_special(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_special(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_flat_ground(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_snow_layer(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_chest(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_torch(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_torch_with_angle(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3f &vertex_pos, float ax, float az);
int render_cactus(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_door(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_cross(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);
int render_slab(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos);

int get_chest_texture_index(Block *block, const Vec3i &pos, uint8_t face);

#endif
#ifndef RENDER_FLUIDS_HPP
#define RENDER_FLUIDS_HPP

#include <cstdint>
#include <math/vec3i.hpp>
#include <gertex/displaylist.hpp>

class Block;
class World;
struct DisplayList;

Vec3f get_fluid_direction(World *world, Block *block, Vec3i pos);
int render_fluid(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos, World *world);
#endif
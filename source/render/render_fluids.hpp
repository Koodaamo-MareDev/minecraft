#ifndef RENDER_FLUIDS_HPP
#define RENDER_FLUIDS_HPP

#include <cstdint>
#include <math/vec3i.hpp>

class Chunk;
class Block;
class World;
Vec3f get_fluid_direction(World *world, Block *block, Vec3i pos);
int render_fluid(Block *block, const Vec3i &pos, World *world);
int render_section_fluids(Chunk &chunk, int index, bool transparent, int vertexCount);
#endif
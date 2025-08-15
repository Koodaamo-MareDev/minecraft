#ifndef RENDER_FLUIDS_HPP
#define RENDER_FLUIDS_HPP

#include <cstdint>
#include <math/vec3i.hpp>

class Chunk;
class Block;
int render_fluid(Chunk &chunk, Block *block, const Vec3i &pos);
int render_section_fluids(Chunk &chunk, int index, bool transparent, int vertexCount);
#endif
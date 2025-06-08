#ifndef RENDER_FLUIDS_HPP
#define RENDER_FLUIDS_HPP

#include <cstdint>
#include <math/vec3i.hpp>

class chunk_t;
class block_t;
int render_fluid(chunk_t &chunk, block_t *block, const vec3i &pos);
int render_fluids(chunk_t &chunk, int section, bool transparent, int vertexCount);
#endif
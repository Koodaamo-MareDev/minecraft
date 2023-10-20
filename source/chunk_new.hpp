#ifndef _CHUNK_NEW_HPP_
#define _CHUNK_NEW_HPP_

#include "vec3i.hpp"
#include "vec3f.hpp"
#include "block.hpp"
#include <ogc/gu.h>
#include <cstddef>
#define RENDER_DISTANCE 6
#define CHUNK_COUNT (RENDER_DISTANCE * RENDER_DISTANCE)
#define VERTICAL_SECTION_COUNT 16
#define VBO_COUNT (CHUNK_COUNT * VERTICAL_SECTION_COUNT)

#define VBO_SOLID 1
#define VBO_TRANSPARENT 2
#define VBO_ALL 0xFF

class chunkvbo_t
{
public:
    bool visible = false;
    bool dirty = false;
    int x = 0;
    unsigned char y = 0;
    int z = 0;
    int solid_buffer_length = 0;
    int transparent_buffer_length = 0;
    void *solid_buffer = nullptr;
    void *transparent_buffer = nullptr;
    int cached_solid_buffer_length = 0;
    int cached_transparent_buffer_length = 0;
    void *cached_solid_buffer = nullptr;
    void *cached_transparent_buffer = nullptr;
};

class chunk_t
{
public:
    bool valid = false;
    int x = 0;
    int z = 0;
    block_t blockstates[16 * 16 * 256] = {0};
    chunkvbo_t vbos[VERTICAL_SECTION_COUNT];
    chunkvbo_t transparent_vbos[VERTICAL_SECTION_COUNT];
    vec3f cave_noise_direction = {30., 30., 30.};
    block_t* get_block(int x, int y, int z);
    void update_blockI(int index);
    void update_block(vec3i local_pos);
    void recalculate();
    void recalculate_section(int section);
    void build_all_vbos();
    void destroy_vbo(int section, unsigned char which);
    void rebuild_vbo(int section, bool transparent);
    int build_vbo(int section, bool transparent);
    int render_fluid(block_t *block, vec3i pos);
    void prepare_render();
    int pre_render_fluid_mesh(int section, bool transparent);
    int render_fluid_mesh(int section, bool transparent, int vertexCount);

private:
};

struct vertex_property_t
{
    vec3f pos;
    uint8_t x_uv;
    uint8_t y_uv;
    uint8_t index;
};
// 0 = -x, -z
// 1 = +x, -z
// 2 = +x, +z
// 3 = -x, +z
extern const vec3i face_offsets[];

extern bool vtx_is_drawing;

chunk_t *get_chunks();
chunkvbo_t **sort_vbos(guVector pos);
void init_chunks();
void prepare_chunks();
BlockID get_block_id_at(vec3i position, BlockID default_id = BlockID::air);
block_t *get_block_at(vec3i vec);
chunk_t *get_chunk(int chunkX, int chunkZ, bool load);
void *get_aligned_pointer_32(void *ptr);
void get_neighbors(vec3i pos, block_t **neighbors);
void lock_chunk_cache(chunk_t *chunk = nullptr);
void unlock_chunk_cache();
void update_block_at(vec3i pos);

void GX_Vertex(vertex_property_t vert, float light);
#endif
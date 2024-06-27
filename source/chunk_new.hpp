#ifndef _CHUNK_NEW_HPP_
#define _CHUNK_NEW_HPP_

#include "vec3i.hpp"
#include "vec3f.hpp"
#include "block.hpp"
#include "entity.hpp"
#include <ogc/mutex.h>
#include <ogc/gu.h>
#include <cstddef>
#include <vector>
#include <deque>
#define SIMULATION_DISTANCE 2
#define RENDER_DISTANCE 4
#define CHUNK_COUNT ((RENDER_DISTANCE) * (RENDER_DISTANCE + 1) * 4)
#define GENERATION_DISTANCE ((RENDER_DISTANCE - 1) * 16)
#define VERTICAL_SECTION_COUNT 16

#define WORLDGEN_TREE_ATTEMPTS 8

#define VBO_SOLID 1
#define VBO_TRANSPARENT 2
#define VBO_ALL 0xFF

extern guVector player_pos;

class chunkvbo_t
{
public:
    bool visible = false;
    bool dirty = false;
    int32_t x = 0;
    uint8_t y = 0;
    int32_t z = 0;
    uint32_t solid_buffer_length = 0;
    uint32_t transparent_buffer_length = 0;
    void *solid_buffer = nullptr;
    void *transparent_buffer = nullptr;
    uint32_t cached_solid_buffer_length = 0;
    uint32_t cached_transparent_buffer_length = 0;
    void *cached_solid_buffer = nullptr;
    void *cached_transparent_buffer = nullptr;

    float player_taxicab_distance() {
        return std::abs(this->x - player_pos.x) + std::abs(this->y - player_pos.y) + std::abs(this->z - player_pos.z);
    }

    bool operator< (chunkvbo_t& other)
    {
        return this->player_taxicab_distance() < other.player_taxicab_distance();
    }
};

class chunk_t
{
public:
    bool valid = false;
    int x = 0;
    int z = 0;
    uint32_t chunk_seed_x;
    uint32_t chunk_seed_z;
    uint8_t lit_state = 0;
    block_t blockstates[16 * 16 * 256] = {0};
    uint8_t height_map[16 * 16] = {0};
    chunkvbo_t vbos[VERTICAL_SECTION_COUNT];
    uint8_t has_fluid_updates[VERTICAL_SECTION_COUNT] = {1};
    std::vector<aabb_entity_t *> entities;

    block_t *get_block(vec3i pos)
    {
        return &this->blockstates[(pos.x & 0xF) | ((pos.y & 0xFF) << 8) | ((pos.z & 0xF) << 4)];
    }

    void set_block(vec3i pos, BlockID block_id)
    {
        this->blockstates[(pos.x & 0xF) | ((pos.y & 0xFF) << 8) | ((pos.z & 0xF) << 4)].set_blockid(block_id);
    }

    void replace_air(vec3i position, BlockID id)
    {
        block_t *block = this->get_block(position);
        if (block->get_blockid() != BlockID::air)
            return;
        block->set_blockid(id);
    }
    
    float player_taxicab_distance() {
        return std::abs(this->x - player_pos.x) + std::abs(this->z - player_pos.z);
    }

    bool operator< (chunk_t& other)
    {
        return this->player_taxicab_distance() < other.player_taxicab_distance();
    }

    void update_height_map(vec3i pos);
    void recalculate();
    void light_up();
    void recalculate_section(int section);
    void build_all_vbos();
    void destroy_vbo(int section, unsigned char which);
    void rebuild_vbo(int section, bool transparent);
    int build_vbo(int section, bool transparent);
    int render_fluid(block_t *block, vec3i pos);
    void prepare_render();
    int pre_render_fluid_mesh(int section, bool transparent);
    int render_fluid_mesh(int section, bool transparent, int vertexCount);
    int pre_render_block_mesh(int section, bool transparent);
    int render_block_mesh(int section, bool transparent, int vertexCount);
    int render_block(block_t *block, vec3i pos, bool transparent);
    int render_torch(block_t *block, vec3i pos);

    void update_entities(float dt);

    uint32_t size();
    chunk_t()
    {
    }

private:
};
// 0 = -x, -z
// 1 = +x, -z
// 2 = +x, +z
// 3 = -x, +z
extern const vec3i face_offsets[];
extern mutex_t chunk_mutex;
std::deque<chunk_t *> &get_chunks();
void init_chunks();
void deinit_chunks();
void print_chunk_status();
bool has_pending_chunks();
BlockID get_block_id_at(vec3i position, BlockID default_id = BlockID::air, chunk_t *near = nullptr);
block_t *get_block_at(vec3i vec, chunk_t *near = nullptr);
vec3i block_to_chunk_pos(vec3i pos);
chunk_t *get_chunk_from_pos(vec3i pos, bool load, bool write_cache = true);
chunk_t *get_chunk(vec3i pos, bool load, bool write_cache = true);
void add_chunk(vec3i pos);
void generate_chunk();
void *get_aligned_pointer_32(void *ptr);
void get_neighbors(vec3i pos, block_t **neighbors);
void update_block_at(vec3i pos);
void update_neighbors(vec3i pos);
#endif
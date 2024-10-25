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

enum class ChunkGenStage : uint8_t
{
    empty = 0,
    heightmap = 1,
    cavegen = 2,
    features = 3,
    done = 4,

    // Special values
    invalid = 0xFF,
};

extern guVector player_pos;

class vbo_buffer_t
{
public:
    void *buffer;
    uint32_t length;

    vbo_buffer_t() : buffer(nullptr), length(0)
    {
    }

    vbo_buffer_t(void *buffer, uint32_t length) : buffer(buffer), length(length)
    {
    }

    bool operator==(vbo_buffer_t &other)
    {
        return this->buffer == other.buffer && this->length == other.length;
    }

    bool operator!=(vbo_buffer_t &other)
    {
        return this->buffer != other.buffer || this->length != other.length;
    }

    operator bool()
    {
        return this->buffer != nullptr && this->length > 0;
    }

    // Detach the buffer from the object without freeing it
    void detach()
    {
        this->buffer = nullptr;
        this->length = 0;
    }

    // Free the buffer and set the object to an empty state
    void clear()
    {
        if (this->buffer)
        {
            free(this->buffer);
            this->buffer = nullptr;
        }
        this->length = 0;
    }
};

class chunkvbo_t
{
public:
    bool visible = false;
    bool dirty = false;
    int32_t x = 0;
    uint8_t y = 0;
    int32_t z = 0;
    vbo_buffer_t solid;
    vbo_buffer_t cached_solid;
    vbo_buffer_t transparent;
    vbo_buffer_t cached_transparent;

    bool has_solid_fluid = false;
    bool has_transparent_fluid = false;

    float player_taxicab_distance()
    {
        return std::abs((this->x & ~15) - (int(player_pos.x) & ~15)) + std::abs((this->y & ~15) - (int(player_pos.y) & ~15)) + std::abs((this->z & ~15) - (int(player_pos.z) & ~15));
    }

    bool operator<(chunkvbo_t &other)
    {
        return this->player_taxicab_distance() < other.player_taxicab_distance();
    }
};

class chunk_t
{
public:
    int x = 0;
    int z = 0;
    ChunkGenStage generation_stage = ChunkGenStage::empty;
    uint32_t chunk_seed_x;
    uint32_t chunk_seed_z;
    uint8_t lit_state = 0;
    block_t blockstates[16 * 16 * 256] = {0};
    uint8_t height_map[16 * 16] = {0};
    uint8_t terrain_map[16 * 16] = {0};
    chunkvbo_t vbos[VERTICAL_SECTION_COUNT];
    uint8_t has_fluid_updates[VERTICAL_SECTION_COUNT] = {1};
    uint32_t light_update_count = 0;
    std::vector<aabb_entity_t *> entities;

    /**
     * Get block - wraps around the chunk if out of bounds
     * @param pos - the position of the block
     * @return the block at the position
     * NOTE: No bounds checking is done.
     * This returns the wrong block if the position is out of bounds.
     * You might want to use try_get_block instead.
     */
    block_t *get_block(const vec3i &pos)
    {
        return &this->blockstates[(pos.x & 0xF) | ((pos.y & 0xFF) << 8) | ((pos.z & 0xF) << 4)];
    }

    /**
     * Get block - returns nullptr if out of bounds
     * @param pos - the position of the block
     * @return the block at the position or nullptr if out of bounds
     */
    block_t *try_get_block(const vec3i &pos)
    {
        if (block_to_chunk_pos(pos) != vec3i(this->x, 0, this->z))
            return nullptr;
        return &this->blockstates[(pos.x & 0xF) | ((pos.y & 0xFF) << 8) | ((pos.z & 0xF) << 4)];
    }

    /**
     * Set block - wraps around the chunk if out of bounds
     * @param pos - the position of the block
     * @param block_id - the block id to set
     * NOTE: No bounds checking is done.
     * This sets the wrong block if the position is out of bounds.
     * You might want to use try_set_block instead.
     */
    void set_block(const vec3i &pos, BlockID block_id)
    {
        this->blockstates[(pos.x & 0xF) | ((pos.y & 0xFF) << 8) | ((pos.z & 0xF) << 4)].set_blockid(block_id);
    }

    /**
     * Set block - does nothing if the block is out of bounds.
     * @param pos - the position of the block
     * @param block_id - the block id to set
     */
    void try_set_block(const vec3i &pos, BlockID block_id)
    {
        if (block_to_chunk_pos(pos) != vec3i(this->x, 0, this->z))
            return;
        this->blockstates[(pos.x & 0xF) | ((pos.y & 0xFF) << 8) | ((pos.z & 0xF) << 4)].set_blockid(block_id);
    }

    /**
     * Replace air block - does nothing if the block is not air.
     * NOTE: No bounds checking is done.
     * This sets the wrong block if the position is out of bounds.
     * You might want to use try_replace_air instead.
     */
    void replace_air(const vec3i &position, BlockID id)
    {
        block_t *block = this->get_block(position);
        if (block->get_blockid() != BlockID::air)
            return;
        block->set_blockid(id);
    }

    /**
     * Replace air block - does nothing if the block is not air.
     * @param position - the position of the block
     * @param id - the block id to set
     * This does nothing if the block is out of bounds.
     */
    void try_replace_air(const vec3i &position, BlockID id)
    {
        block_t *block = this->try_get_block(position);
        if (!block || block->get_blockid() != BlockID::air)
            return;
        block->set_blockid(id);
    }

    float player_taxicab_distance()
    {
        return std::abs((this->x << 4) - (int(player_pos.x) & ~15)) + std::abs((this->z << 4) - (int(player_pos.z) & ~15));
    }

    bool operator<(chunk_t &other)
    {
        return this->player_taxicab_distance() < other.player_taxicab_distance();
    }

    void update_height_map(vec3i pos);
    void light_up();
    void recalculate_visibility(block_t *block, vec3i pos);
    void recalculate_section_visibility(int section);
    void build_all_vbos();
    void destroy_vbo(int section, unsigned char which);
    void rebuild_vbo(int section, bool transparent);
    int build_vbo(int section, bool transparent);
    int render_fluid(block_t *block, const vec3i &pos);
    int pre_render_fluid_mesh(int section, bool transparent);
    int render_fluid_mesh(int section, bool transparent, int vertexCount);
    int pre_render_block_mesh(int section, bool transparent);
    int render_block_mesh(int section, bool transparent, int vertexCount);
    int pre_render_block(block_t *block, const vec3i &pos, bool transparent);
    int render_block(block_t *block, const vec3i &pos, bool transparent);
    int render_special(block_t *block, const vec3i &pos);
    int render_flat_ground(block_t *block, const vec3i &pos);
    int render_torch(block_t *block, const vec3i &pos);
    int render_cross(block_t *block, const vec3i &pos);
    int render_slab(block_t *block, const vec3i &pos);

    void update_entities();

    void render_entities(float partial_ticks);

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
BlockID get_block_id_at(const vec3i &position, BlockID default_id = BlockID::air, chunk_t *near = nullptr);
block_t *get_block_at(const vec3i &vec, chunk_t *near = nullptr);
void set_block_at(const vec3i &pos, BlockID id, chunk_t *near = nullptr);
void replace_air_at(vec3i pos, BlockID id, chunk_t *near = nullptr);
vec3i block_to_chunk_pos(vec3i pos);
chunk_t *get_chunk_from_pos(const vec3i &pos, bool load, bool write_cache = true);
chunk_t *get_chunk(const vec3i &pos, bool load, bool write_cache = true);
void add_chunk(vec3i pos);
void generate_chunk();
void *get_aligned_pointer_32(void *ptr);
void get_neighbors(const vec3i &pos, block_t **neighbors, chunk_t *near = nullptr);
void update_block_at(const vec3i &pos);
void update_neighbors(const vec3i &pos);
vec3f get_fluid_direction(block_t *block, vec3i pos, chunk_t *chunk = nullptr);
#endif
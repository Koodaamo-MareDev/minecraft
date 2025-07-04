#ifndef _CHUNK_NEW_HPP_
#define _CHUNK_NEW_HPP_

#include <ogc/mutex.h>
#include <ogc/gu.h>
#include <cstddef>
#include <vector>
#include <deque>
#include <map>
#include <iostream>
#include <math/vec2i.hpp>
#include <math/vec3i.hpp>
#include <math/vec3f.hpp>
#include "util/constants.hpp"
#include "block.hpp"
#include "entity.hpp"

enum class ChunkGenStage : uint8_t
{
    empty = 0,
    features = 1,
    done = 2,

    // Special values
    loading = 128,
    invalid = 0xFF,
};

extern guVector player_pos;
extern uint32_t world_tick;

inline vec2i block_to_chunk_pos(const vec3i &pos)
{
    return vec2i((pos.x & ~0xF) >> 4, (pos.z & ~0xF) >> 4);
}

class vbo_buffer_t
{
public:
    void *buffer = nullptr;
    uint32_t length = 0;

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

class section
{
public:
    bool visible = false;
    bool dirty = false;
    int32_t x = 0;
    uint8_t y = 0;
    int32_t z = 0;
    chunk_t *chunk = nullptr;
    vbo_buffer_t solid = vbo_buffer_t();
    vbo_buffer_t cached_solid = vbo_buffer_t();
    vbo_buffer_t transparent = vbo_buffer_t();
    vbo_buffer_t cached_transparent = vbo_buffer_t();
    uint32_t visibility_flags = 0;

    bool has_solid_fluid = false;
    bool has_transparent_fluid = false;

    bool has_updated = false;

    int player_taxicab_distance()
    {
        return std::abs((this->x & ~15) - (int(std::floor(player_pos.x)) & ~15)) + std::abs((this->y & ~15) - (int(std::floor(player_pos.y)) & ~15)) + std::abs((this->z & ~15) - (int(std::floor(player_pos.z)) & ~15));
    }

    bool operator<(section &other)
    {
        return this->player_taxicab_distance() < other.player_taxicab_distance();
    }
};

class NBTTagCompound;

class chunk_t
{
public:
    int x = 0;
    int z = 0;
    ChunkGenStage generation_stage = ChunkGenStage::empty;
    uint8_t lit_state = 0;
    block_t blockstates[16 * 16 * WORLD_HEIGHT] = {0};
    uint8_t height_map[16 * 16] = {0};
    uint8_t terrain_map[16 * 16] = {0};
    section sections[VERTICAL_SECTION_COUNT] = {0};
    uint8_t has_fluid_updates[VERTICAL_SECTION_COUNT] = {1};
    uint32_t light_update_count = 0;
    std::vector<entity_physical *> entities;

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
        return &this->blockstates[(pos.x & 0xF) | ((pos.y & MAX_WORLD_Y) << 8) | ((pos.z & 0xF) << 4)];
    }

    /**
     * Get block - returns nullptr if out of bounds
     * @param pos - the position of the block
     * @return the block at the position or nullptr if out of bounds
     */
    block_t *try_get_block(const vec3i &pos)
    {
        if (block_to_chunk_pos(pos) != vec2i(this->x, this->z))
            return nullptr;
        return &this->blockstates[(pos.x & 0xF) | ((pos.y & MAX_WORLD_Y) << 8) | ((pos.z & 0xF) << 4)];
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
        this->blockstates[(pos.x & 0xF) | ((pos.y & MAX_WORLD_Y) << 8) | ((pos.z & 0xF) << 4)].set_blockid(block_id);
    }

    /**
     * Set block - does nothing if the block is out of bounds.
     * @param pos - the position of the block
     * @param block_id - the block id to set
     */
    void try_set_block(const vec3i &pos, BlockID block_id)
    {
        if (block_to_chunk_pos(pos) != vec2i(this->x, this->z))
            return;
        this->blockstates[(pos.x & 0xF) | ((pos.y & MAX_WORLD_Y) << 8) | ((pos.z & 0xF) << 4)].set_blockid(block_id);
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

    int32_t player_taxicab_distance()
    {
        return std::abs((this->x << 4) - (int32_t(std::floor(player_pos.x)) & ~15)) + std::abs((this->z << 4) - (int32_t(std::floor(player_pos.z)) & ~15));
    }

    bool operator<(chunk_t &other)
    {
        return this->player_taxicab_distance() < other.player_taxicab_distance();
    }

    void update_height_map(vec3i pos);
    void light_up();
    void recalculate_height_map();
    void recalculate_visibility(block_t *block, vec3i pos);
    void refresh_section_block_visibility(int index);
    static void init_floodfill_startpoints();
    void vbo_visibility_flood_fill(vec3i pos);
    void refresh_section_visibility(int index);
    int build_vbo(int index, bool transparent);
    void update_entities();

    void render_entities(float partial_ticks, bool transparency);

    uint32_t size();
    chunk_t(int32_t x, int32_t z) : x(x), z(z)
    {
        for (int y = 0; y < VERTICAL_SECTION_COUNT; y++)
        {
            this->sections[y].x = x << 4;
            this->sections[y].y = y << 4;
            this->sections[y].z = z << 4;
            this->sections[y].chunk = this;
        }
    }
    ~chunk_t();

    void save(NBTTagCompound &stream);
    void load(NBTTagCompound &stream);
    void write();
    void read();

private:
};
class chunkprovider;

extern const vec3i face_offsets[];
extern mutex_t chunk_mutex;
std::deque<chunk_t *> &get_chunks();
void init_chunk_manager(chunkprovider *chunk_provider);
void deinit_chunk_manager();
void set_world_hell(bool hell);
BlockID get_block_id_at(const vec3i &position, BlockID default_id = BlockID::air, chunk_t *near = nullptr);
block_t *get_block_at(const vec3i &vec, chunk_t *near = nullptr);
void set_block_at(const vec3i &pos, BlockID id, chunk_t *near = nullptr);
void replace_air_at(vec3i pos, BlockID id, chunk_t *near = nullptr);
chunk_t *get_chunk_from_pos(const vec3i &pos);
chunk_t *get_chunk(int32_t x, int32_t z);
chunk_t *get_chunk(const vec2i &pos);
bool add_chunk(int32_t x, int32_t z);
void get_neighbors(const vec3i &pos, block_t **neighbors, chunk_t *near = nullptr);
void update_block_at(const vec3i &pos);
void update_neighbors(const vec3i &pos);
vec3f get_fluid_direction(block_t *block, vec3i pos, chunk_t *chunk = nullptr);
std::map<int32_t, entity_physical *> &get_entities();
void add_entity(entity_physical *entity);
void remove_entity(int32_t entity_id);
entity_physical *get_entity_by_id(int32_t entity_id);
#endif
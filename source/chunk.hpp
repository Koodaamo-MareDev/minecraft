#ifndef _CHUNK_NEW_HPP_
#define _CHUNK_NEW_HPP_

#include <ogc/mutex.h>
#include <ogc/gu.h>
#include <cstddef>
#include <vector>
#include <deque>
#include <map>
#include <iostream>
#include <functional>
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

inline Vec2i block_to_chunk_pos(const Vec3i &pos)
{
    return Vec2i((pos.x & ~0xF) >> 4, (pos.z & ~0xF) >> 4);
}

class VBO
{
public:
    void *buffer = nullptr;
    uint32_t length = 0;

    VBO() : buffer(nullptr), length(0)
    {
    }

    VBO(void *buffer, uint32_t length) : buffer(buffer), length(length)
    {
    }

    bool operator==(VBO &other)
    {
        return this->buffer == other.buffer && this->length == other.length;
    }

    bool operator!=(VBO &other)
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

class Section
{
public:
    bool visible = false;
    bool dirty = false;
    int32_t x = 0;
    uint8_t y = 0;
    int32_t z = 0;
    Chunk *chunk = nullptr;
    VBO solid = VBO();
    VBO cached_solid = VBO();
    VBO transparent = VBO();
    VBO cached_transparent = VBO();
    uint16_t visibility_flags = 0;

    bool has_solid_fluid = false;
    bool has_transparent_fluid = false;

    bool has_updated = false;
};

class NBTTagCompound;

class Chunk
{
public:
    int x = 0;
    int z = 0;
    ChunkGenStage generation_stage = ChunkGenStage::empty;
    uint8_t lit_state = 0;
    Block blockstates[16 * 16 * WORLD_HEIGHT] = {0};
    uint8_t height_map[16 * 16] = {0};
    uint8_t terrain_map[16 * 16] = {0};
    Section sections[VERTICAL_SECTION_COUNT] = {0};
    uint8_t has_fluid_updates[VERTICAL_SECTION_COUNT] = {1};
    uint32_t light_update_count = 0;
    std::vector<EntityPhysical *> entities;

    /**
     * Get block - wraps around the chunk if out of bounds
     * @param pos - the position of the block
     * @return the block at the position
     * NOTE: No bounds checking is done.
     * This returns the wrong block if the position is out of bounds.
     * You might want to use try_get_block instead.
     */
    Block *get_block(const Vec3i &pos)
    {
        return &this->blockstates[(pos.x & 0xF) | ((pos.y & MAX_WORLD_Y) << 8) | ((pos.z & 0xF) << 4)];
    }

    /**
     * Get block - returns nullptr if out of bounds
     * @param pos - the position of the block
     * @return the block at the position or nullptr if out of bounds
     */
    Block *try_get_block(const Vec3i &pos)
    {
        if (block_to_chunk_pos(pos) != Vec2i(this->x, this->z))
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
    void set_block(const Vec3i &pos, BlockID block_id)
    {
        this->blockstates[(pos.x & 0xF) | ((pos.y & MAX_WORLD_Y) << 8) | ((pos.z & 0xF) << 4)].set_blockid(block_id);
    }

    /**
     * Set block - does nothing if the block is out of bounds.
     * @param pos - the position of the block
     * @param block_id - the block id to set
     */
    void try_set_block(const Vec3i &pos, BlockID block_id)
    {
        if (block_to_chunk_pos(pos) != Vec2i(this->x, this->z))
            return;
        this->blockstates[(pos.x & 0xF) | ((pos.y & MAX_WORLD_Y) << 8) | ((pos.z & 0xF) << 4)].set_blockid(block_id);
    }

    /**
     * Replace air block - does nothing if the block is not air.
     * NOTE: No bounds checking is done.
     * This sets the wrong block if the position is out of bounds.
     * You might want to use try_replace_air instead.
     */
    void replace_air(const Vec3i &position, BlockID id)
    {
        Block *block = this->get_block(position);
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
    void try_replace_air(const Vec3i &position, BlockID id)
    {
        Block *block = this->try_get_block(position);
        if (!block || block->get_blockid() != BlockID::air)
            return;
        block->set_blockid(id);
    }

    int32_t player_taxicab_distance();

    bool operator<(Chunk &other)
    {
        return this->player_taxicab_distance() < other.player_taxicab_distance();
    }

    void update_height_map(Vec3i pos);
    void light_up();
    void recalculate_height_map();
    void recalculate_visibility(Block *block, Vec3i pos);
    void refresh_section_block_visibility(int index);
    static void init_floodfill_startpoints();
    void vbo_visibility_flood_fill(Vec3i pos);
    void refresh_section_visibility(int index);
    int build_vbo(int index, bool transparent);
    void update_entities();

    void render_entities(float partial_ticks, bool transparency);

    uint32_t size();
    Chunk(int32_t x, int32_t z) : x(x), z(z)
    {
        for (int y = 0; y < VERTICAL_SECTION_COUNT; y++)
        {
            this->sections[y].x = x << 4;
            this->sections[y].y = y << 4;
            this->sections[y].z = z << 4;
            this->sections[y].chunk = this;
        }
    }
    ~Chunk();

    void save(NBTTagCompound &stream);
    void load(NBTTagCompound &stream);
    void write();
    void read();

private:
};
class ChunkProvider;

extern const Vec3i face_offsets[];
extern mutex_t chunk_mutex;
std::deque<Chunk *> &get_chunks();
void init_chunk_manager(ChunkProvider *chunk_provider);
void deinit_chunk_manager();
void set_world_hell(bool hell);
BlockID get_block_id_at(const Vec3i &position, BlockID default_id = BlockID::air, Chunk *near = nullptr);
Block *get_block_at(const Vec3i &vec, Chunk *near = nullptr);
void set_block_at(const Vec3i &pos, BlockID id, Chunk *near = nullptr);
void replace_air_at(Vec3i pos, BlockID id, Chunk *near = nullptr);
Chunk *get_chunk_from_pos(const Vec3i &pos);
Chunk *get_chunk(int32_t x, int32_t z);
Chunk *get_chunk(const Vec2i &pos);
bool add_chunk(int32_t x, int32_t z);
void remove_chunk(Chunk *chunk);
void remove_chunks_if(std::function<bool(Chunk *)> predicate);
void get_neighbors(const Vec3i &pos, Block **neighbors, Chunk *near = nullptr);
void update_block_at(const Vec3i &pos);
void update_neighbors(const Vec3i &pos);
Vec3f get_fluid_direction(Block *block, Vec3i pos, Chunk *chunk = nullptr);
std::map<int32_t, EntityPhysical *> &get_entities();
void add_entity(EntityPhysical *entity);
void remove_entity(int32_t entity_id);
EntityPhysical *get_entity_by_id(int32_t entity_id);
#endif
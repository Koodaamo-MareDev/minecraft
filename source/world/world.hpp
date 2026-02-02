#ifndef WORLD_HPP
#define WORLD_HPP

#include <string>
#include <math/vec3i.hpp>
#include <math/vec2i.hpp>
#include <math/vec3f.hpp>
#include <math/math_utils.h>
#include <crapper/client.hpp>
#include <set>
#include <map>

#include <world/particle.hpp>
#include "sound.hpp"
#include <item/inventory.hpp>
#include <block/block_tick.hpp>

class Chunk;
class EntityPhysical;
class AABB;
class Camera;
class ChunkProvider;
class Frustum;
class TileEntity;
struct Progress;

enum class SectionUpdatePhase : uint8_t
{
    BLOCK_VISIBILITY = 0,   // Update visibility of blocks in a section
    SECTION_VISIBILITY = 1, // Update visibility of sections in world
    SOLID = 2,              // Update solid VBOs
    TRANSPARENT = 3,        // Update transparent VBOs
    FLUSH = 4,              // Flush all VBOs
    COUNT = 5               // Total number of phases
};

inline SectionUpdatePhase operator++(SectionUpdatePhase &phase, int)
{
    SectionUpdatePhase old_phase = phase;
    if (uint8_t(old_phase) + 1 >= uint8_t(SectionUpdatePhase::COUNT))
        phase = SectionUpdatePhase::BLOCK_VISIBILITY;
    else
        phase = SectionUpdatePhase(uint8_t(old_phase) + 1);
    return old_phase;
};

inline SectionUpdatePhase &operator++(SectionUpdatePhase &phase)
{
    if (uint8_t(phase) + 1 >= uint8_t(SectionUpdatePhase::COUNT))
        phase = SectionUpdatePhase::BLOCK_VISIBILITY;
    else
        phase = SectionUpdatePhase(uint8_t(phase) + 1);
    return phase;
}

class World
{
public:
    uint32_t ticks = 0;
    int time_of_day = 0;
    int last_entity_tick = 0;
    int last_fluid_tick = 0;
    int fluid_update_count = 0;
    double delta_time = 0.0;
    double partial_ticks = 0.0;
    size_t memory_usage = 0;
    Vec3i spawn_pos = Vec3i(0, 64, 0);
    EntityPlayerLocal player = EntityPlayerLocal(Vec3f(0, -999, 0));
    bool loaded = false;
    bool hell = false;
    int64_t seed = 0;
    ChunkProvider *chunk_provider = nullptr;
    SectionUpdatePhase current_update_phase = SectionUpdatePhase::BLOCK_VISIBILITY;
    bool sync_chunk_updates = false;
    bool smooth_lighting = false;

    std::map<int32_t, EntityPhysical *> world_entities;
    std::deque<Chunk *> chunks;
    std::map<uint64_t, Chunk *> chunk_cache;
    std::deque<Chunk *> pending_chunks;
    mutex_t chunk_mutex = LWP_MUTEX_NULL;
    lwp_t chunk_manager_thread_handle = LWP_THREAD_NULL;
    bool run_chunk_manager = false;

    Frustum *frustum = nullptr;
    Crapper::MinecraftClient *client = nullptr;
    std::set<BlockTick> scheduled_updates;
    mutex_t tick_mutex = LWP_MUTEX_NULL;

    std::string name = "world";

    World(std::string name);
    World();
    ~World();

    bool is_remote();
    void set_remote(bool value);

    bool tick();
    void update();
    void update_frustum(Camera &camera);
    void update_chunks();
    SectionUpdatePhase update_sections(SectionUpdatePhase phase);
    void calculate_visibility();
    void tick_blocks();
    void schedule_block_update(const Vec3i &pos, BlockID id, int ticks);
    void edit_blocks();

    int prepare_chunks(int count);
    void save_and_clean_chunk(Chunk *chunk);

    void add_particle(const Particle &particle);
    void play_sound(const Sound &sound);
    void destroy_block(const Vec3i pos, Block *old_block);
    bool place_block(const Vec3i pos, const Vec3i targeted, Block *new_block, uint8_t face);
    void spawn_drop(const Vec3i &pos, const Block *old_block, item::ItemStack item);
    void create_explosion(Vec3f pos, float power);

    void init_chunk_manager(ChunkProvider *chunk_provider);
    void deinit_chunk_manager();
    void set_hell(bool hell);
    BlockID get_block_id_at(const Vec3i &position, BlockID default_id = BlockID::air);
    Block *get_block_at(const Vec3i &vec);
    uint8_t get_meta_at(const Vec3i &position);
    void set_block_at(const Vec3i &pos, BlockID id);
    void set_meta_at(const Vec3i &pos, uint8_t meta);
    void set_block_and_meta_at(const Vec3i &pos, BlockID id, uint8_t meta);
    void replace_air_at(Vec3i pos, BlockID id);
    TileEntity *get_tile_entity(const Vec3i &position);
    Chunk *get_chunk_from_pos(const Vec3i &pos);
    Chunk *get_chunk(int32_t x, int32_t z);
    Chunk *get_chunk(const Vec2i &pos);
    bool add_chunk(int32_t x, int32_t z);
    void save_chunk(Chunk *chunk);
    void get_neighbors(const Vec3i &pos, Block **neighbors);
    void notify_at(const Vec3i &pos);
    void mark_block_dirty(const Vec3i &pos);
    void notify_neighbors(const Vec3i &pos);
    void add_entity(EntityPhysical *entity);
    void remove_entity(int32_t entity_id);
    EntityPhysical *get_entity_by_id(int32_t entity_id);

    void draw(Camera &camera);
    void draw_scene(bool opaque);
    void draw_selected_block();
    void draw_bounds(AABB *bounds);

    void save(Progress *prog = nullptr);
    bool load();
    void create();

    void set_sound_system(SoundSystem *sound_system);

private:
    ParticleSystem m_particle_system = ParticleSystem(this);
    SoundSystem *m_sound_system = nullptr;

    void update_entities();
    void update_player();
};

#endif
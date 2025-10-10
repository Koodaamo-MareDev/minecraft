#ifndef WORLD_HPP
#define WORLD_HPP

#include <string>
#include <math/vec3i.hpp>
#include <math/vec3f.hpp>
#include <math/math_utils.h>
#include <crapper/client.hpp>
#include <set>

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
    void remove_chunk(Chunk *chunk);
    void cleanup_chunks();

    void add_particle(const Particle &particle);
    void play_sound(const Sound &sound);
    void destroy_block(const Vec3i pos, Block *old_block);
    bool place_block(const Vec3i pos, const Vec3i targeted, Block *new_block, uint8_t face);
    void spawn_drop(const Vec3i &pos, const Block *old_block, inventory::ItemStack item);
    void create_explosion(Vec3f pos, float power);

    void draw(Camera &camera);
    void draw_scene(bool opaque);
    void draw_selected_block();
    void draw_bounds(AABB *bounds);

    void save();
    bool load();
    void create();

private:
    ParticleSystem m_particle_system;
    SoundSystem m_sound_system;

    void update_entities();
    void update_player();
};

extern World *current_world;

#endif
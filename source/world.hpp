#ifndef WORLD_HPP
#define WORLD_HPP

#include <string>
#include <math/vec3i.hpp>
#include <math/vec3f.hpp>
#include <math/math_utils.h>

#include "particle.hpp"
#include "sound.hpp"
#include "inventory.hpp"

class chunk_t;
class entity_physical;
class aabb_t;
class camera_t;
class chunkprovider;

class player_properties
{
public:
    entity_player_local *m_entity = nullptr;
    BlockID in_fluid = BlockID::air;
    vec3f view_bob_offset = vec3f(0, 0, 0);
    vec3f view_bob_screen_offset = vec3f(0, 0, 0);

    // Raycast related
    vec3i raycast_pos = vec3i(0, 0, 0);
    vec3i raycast_face = vec3i(0, 0, 0);
    std::vector<aabb_t> block_bounds;
    bool draw_block_outline = false;

    inventory::container m_inventory = inventory::container(40, 36); // 4 rows of 9 slots, the rest 4 are the armor slots
    inventory::item_stack *selected_item = nullptr;
    int selected_hotbar_slot = 0;

    player_properties();
};

class world
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
    player_properties player;
    bool loaded = false;
    bool hell = false;
    int64_t seed = 0;
    chunkprovider *chunk_provider = nullptr;

    std::string name = "world";

    world();
    ~world();

    bool is_remote();
    void set_remote(bool value);

    void tick();
    void update();
    void update_chunks();
    void update_vbos();
    void update_fluids(chunk_t *chunk, int section);
    void edit_blocks();

    int prepare_chunks(int count);
    void remove_chunk(chunk_t *chunk);
    void cleanup_chunks();

    void add_particle(const particle &particle);
    void play_sound(const sound &sound);
    void destroy_block(const vec3i pos, block_t *old_block);
    void place_block(const vec3i pos, const vec3i targeted, block_t *new_block, uint8_t face);
    void spawn_drop(const vec3i &pos, const block_t *old_block, inventory::item_stack item);
    void create_explosion(vec3f pos, float power, chunk_t *near);

    void draw(camera_t &camera);
    void draw_scene(bool opaque);
    void draw_selected_block();
    void draw_bounds(aabb_t *bounds);

    void save();
    bool load();
    void create();
    void reset();

private:
    bool remote = false;

    particle_system m_particle_system;
    sound_system m_sound_system;

    void update_entities();
    void update_player();
};

extern world *current_world;

#endif
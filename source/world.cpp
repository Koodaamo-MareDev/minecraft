#include <algorithm>
#include <unistd.h>
#include <ported/Random.hpp>
#include <crapper/client.hpp>

#include "timers.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "block.hpp"
#include "blocks.hpp"
#include "entity.hpp"
#include "render.hpp"
#include "particle.hpp"
#include "mcregion.hpp"
#include "lock.hpp"
#include "raycast.hpp"
#include "nbt/nbt.hpp"
#include "light.hpp"
#include "gui_dirtscreen.hpp"
#include "chunkprovider.hpp"
#include "util/face_pair.hpp"
#include "util/crashfix.hpp"
#include "util/debuglog.hpp"

extern bool should_destroy_block;
extern bool should_place_block;
extern gui *current_gui;
extern int mkpath(const char *path, mode_t mode);

world::world()
{
    std::string save_path = "/apps/minecraft/saves/" + name;
    std::string region_path = save_path + "/region";
    mkpath(region_path.c_str(), 0777);
    chdir(save_path.c_str());

    light_engine::init();

    player.m_entity = new entity_player_local(vec3f(0, -999, 0));
}

world::~world()
{
    light_engine::deinit();
    delete player.m_entity;
    if (chunk_provider)
        delete chunk_provider;
    if (frustum)
        delete frustum;
}

bool world::is_remote()
{
    return client != nullptr;
}

void world::set_remote(bool value)
{
    if (value && !client)
    {
        client = new Crapper::MinecraftClient(this);
    }
    else if (!value && client)
    {
        client->disconnect();
        delete client;
        client = nullptr;
    }
}

void world::tick()
{
    if (client)
    {
        try
        {
            client->tick();
        }
        catch (const std::runtime_error &e)
        {
            debug::print("Network error: %s\n", e.what());

            // Go back to singleplayer
            reset();
            if (!load())
                create();

            // Inform the user about the network error
            gui_dirtscreen *dirtscreen = new gui_dirtscreen(gertex::GXView());
            dirtscreen->set_text(std::string(e.what()));
            gui::set_gui(dirtscreen);
        }
    }
    update_entities();
    calculate_visibility();
    cleanup_chunks();
    update_chunks();

    // Calculate chunk memory usage
    memory_usage = 0;
    for (chunk_t *&chunk : get_chunks())
    {
        memory_usage += chunk ? chunk->size() : 0;
    }

    m_sound_system.update(angles_to_vector(0, yrot + 90), player.m_entity->get_position(std::fmod(partial_ticks, 1)));

    current_world->last_entity_tick = current_world->ticks;
}

void world::update()
{
    static section_update_phase current_update_phase = section_update_phase::BLOCK_VISIBILITY;
    // If there are no chunks loaded, we can skip the update
    uint64_t start_time = time_get();
    uint64_t elapsed_time = 0;

    // Limit to 6ms per update
    while (elapsed_time < 6000)
    {
        // Cycle through the phases
        current_update_phase = update_sections(current_update_phase);
        elapsed_time = time_diff_us(start_time, time_get());
    }
    edit_blocks();
    m_particle_system.update(delta_time);
}

void world::update_frustum(camera_t &camera)
{
    if (!frustum)
        frustum = new frustum_t;
    build_frustum(camera, *frustum);
}

void world::update_chunks()
{
    int light_up_calls = 0;
    for (chunk_t *&chunk : get_chunks())
    {
        if (chunk)
        {
            float hdistance = chunk->player_taxicab_distance();
            if (hdistance > CHUNK_DISTANCE * 24 + 24 && !is_remote())
            {
                remove_chunk(chunk);
                continue;
            }

            if (chunk->generation_stage != ChunkGenStage::done)
                continue;

            // Tick chunks
            for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
            {
                float vdistance = std::abs(j * 16 - player_pos.y);
                if (!is_remote() && chunk->has_fluid_updates[j] && vdistance <= SIMULATION_DISTANCE * 16 && ticks - last_fluid_tick >= 5)
                    update_fluid_section(chunk, j);
            }
            if (!chunk->lit_state && light_up_calls < 5)
            {
                light_up_calls++;
                chunk->light_up();
            }
        }
    }
    if (ticks - last_fluid_tick >= 5)
    {
        last_fluid_tick = ticks;
        fluid_update_count++;
        fluid_update_count %= 30;
    }
    if (!is_remote())
    {
        prepare_chunks(1);
    }
}

void world::update_fluid_section(chunk_t *chunk, int index)
{
    auto int_to_blockpos = [](ptrdiff_t x) -> vec3i
    {
        return vec3i(x & 0xF, (x >> 8) & MAX_WORLD_Y, (x >> 4) & 0xF);
    };

    static std::vector<std::vector<block_t *>> fluid_levels(8);
    static bool init = false;
    if (!init)
    {
        for (size_t i = 0; i < fluid_levels.size(); i++)
        {
            fluid_levels[i].resize(4096);
        }
        init = true;
    }

    int chunkX = (chunk->x * 16);
    int chunkZ = (chunk->z * 16);

    for (size_t i = 0; i < 4096; i++)
    {
        block_t *block = &chunk->blockstates[i | (index << 12)];
        if ((block->meta & FLUID_UPDATE_REQUIRED_FLAG))
            fluid_levels[get_fluid_meta_level(block) & 7][i] = block;
    }

    uint16_t curr_fluid_count = 0;
    for (size_t i = 0; i < fluid_levels.size(); i++)
    {
        std::vector<block_t *> &blocks = fluid_levels[i];
        for (block_t *&block : blocks)
        {
            if (!block)
                continue;
            if ((basefluid(block->get_blockid()) != BlockID::lava || fluid_update_count % 6 == 0))
                update_fluid(block, vec3i(chunkX, 0, chunkZ) + int_to_blockpos(block - chunk->blockstates), chunk);
            curr_fluid_count++;
            block = nullptr;
        }
    }
    chunk->has_fluid_updates[index] = (curr_fluid_count != 0);
}

section_update_phase world::update_sections(section_update_phase phase)
{
    // Buffer to hold VBOs that need to be flushed
    static std::vector<section *> vbos_to_flush;

    // Pointer to the current VBO being processed
    static section *curr_section = nullptr;

    // Gets the VBO at the given position
    auto section_at = [](const vec3i &pos) -> section *
    {
        // Out of bounds check for Y coordinate
        if (pos.y < 0 || pos.y >= WORLD_HEIGHT)
            return nullptr;

        // Get the chunk from the position
        chunk_t *chunk = get_chunk_from_pos(pos);

        // If the chunk doesn't exist, neither does the VBO
        if (!chunk)
            return nullptr;

        return &chunk->sections[pos.y >> 4];
    };

    // Process the current VBO based on the phase
    switch (phase)
    {
    case section_update_phase::BLOCK_VISIBILITY:
    {
        for (chunk_t *&chunk : get_chunks())
        {
            if (chunk && chunk->generation_stage == ChunkGenStage::done && !chunk->light_update_count)
            {
                // Check if chunk has other chunks around it.
                bool surrounding = true;
                for (int i = 0; i < 6; i++)
                {
                    // Skip the top and bottom faces
                    if (i == 2)
                        i = 4;
                    // Check if the surrounding chunk exists and has no lighting updates pending
                    chunk_t *surrounding_chunk = get_chunk(chunk->x + face_offsets[i].x, chunk->z + face_offsets[i].z);
                    if (!surrounding_chunk || surrounding_chunk->light_update_count || surrounding_chunk->generation_stage != ChunkGenStage::done)
                    {
                        surrounding = false;
                        break;
                    }
                }
                // If the chunk has no surrounding chunks, skip it.
                if (!surrounding)
                    continue;
                for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
                {
                    section &other_section = chunk->sections[j];
                    if (other_section.visible && other_section.dirty && (!curr_section || other_section.chunk < curr_section->chunk))
                    {
                        // If the current section is not set or the other section is closer to the player, set it as the current section
                        curr_section = &other_section;
                    }
                }
            }
        }

        if (!curr_section)
        {
            // If no section was set, skip to the next phase
            break;
        }

        // Recalculate the visibility of the section
        curr_section->chunk->refresh_section_block_visibility(curr_section->y >> 4);

        break;
    }
    case section_update_phase::SECTION_VISIBILITY:
        if (!curr_section)
            break;
        curr_section->chunk->refresh_section_visibility(curr_section->y >> 4);
        break;
    case section_update_phase::SOLID:
        if (!curr_section)
            break;
        curr_section->chunk->build_vbo(curr_section->y >> 4, false);
        break;
    case section_update_phase::TRANSPARENT:
        if (!curr_section)
            break;
        curr_section->chunk->build_vbo(curr_section->y >> 4, true);

        // Clear the section's dirty flag
        curr_section->dirty = false;

        // Add the section to the flush buffer
        vbos_to_flush.push_back(curr_section);

        // Reset the section pointer
        curr_section = nullptr;
        break;
    case section_update_phase::FLUSH:
    {
        // Flush cached buffers
        std::vector<section *> skipped_sections;
        for (section *current : vbos_to_flush)
        {
            if (current->dirty)
                continue;
            bool updated_neighbors = true;
            for (int i = 0; i < 6; i++)
            {
                vec3i neighbor_pos = vec3i(current->x, current->y, current->z) + face_offsets[i] * 16;
                section *neighbor = section_at(neighbor_pos);
                if (!neighbor || !neighbor->visible)
                    continue;

                if (neighbor->dirty || neighbor->chunk->light_update_count)
                {
                    // Redo this section
                    curr_section = neighbor;
                    updated_neighbors = false;
                    break;
                }
            }

            if (!updated_neighbors)
            {
                // If the neighbors are not up-to-date, skip this section
                skipped_sections.push_back(current);
                continue;
            }

            if (current->solid != current->cached_solid)
            {
                // Clear the cached buffer
                current->cached_solid.clear();

                // Set the cached buffer to the new buffer
                current->cached_solid = current->solid;
            }
            if (current->transparent != current->cached_transparent)
            {
                // Clear the cached buffer
                current->cached_transparent.clear();

                // Set the cached buffer to the new buffer
                current->cached_transparent = current->transparent;
            }
            current->has_updated = true;
        }
        vbos_to_flush.clear();

        // Add back the sections that had to be skipped
        for (section *skipped : skipped_sections)
        {
            vbos_to_flush.push_back(skipped);
        }
        break;
    }
    default:
        break;
    }
    if (phase++ != section_update_phase::FLUSH && !curr_section)
    {
        // Skip to flush phase if no section is set
        phase = section_update_phase::FLUSH;
    }

    return phase;
}

/**
 * This implementation of cave culling is based on the documentation
 * and the implementation of the original Minecraft culling algorithm.
 * You can find the original 2D implementation here:
 * https://tomcc.github.io/index.html
 */

struct section_node
{
    section *sect;
    int8_t from;     // The face we entered the VBO from
    uint8_t dirs[6]; // For preventing revisits of the same face
};

void world::calculate_visibility()
{
    init_face_pairs();
    // Reset visibility status for all VBOs
    for (chunk_t *&chunk : get_chunks())
    {
        if (chunk)
        {
            for (uint8_t i = 0; i < VERTICAL_SECTION_COUNT; i++)
            {
                chunk->sections[i].visible = false;
            }
        }
    }

    // Gets the VBO at the given position
    auto section_at = [](const vec3i &pos) -> section *
    {
        // Out of bounds check for Y coordinate
        if (pos.y < 0 || pos.y >= WORLD_HEIGHT)
            return nullptr;

        // Get the chunk from the position
        chunk_t *chunk = get_chunk_from_pos(pos);

        // If the chunk doesn't exist, neither does the VBO
        if (!chunk)
            return nullptr;

        return &chunk->sections[pos.y >> 4];
    };

    std::deque<section_node> section_queue;
    std::deque<section *> visited;
    vec3f fpos = player_pos;
    section_node entry;
    entry.sect = section_at(vec3i(int(fpos.x), int(fpos.y), int(fpos.z)));

    // Check if there is a VBO at the player's position
    if (!entry.sect)
        return;

    // Initialize the rest of the entry node
    entry.from = -1;
    for (uint8_t i = 0; i < 6; i++)
        entry.dirs[i] = 0;
    section_queue.push_front(entry);
    visited.push_front(entry.sect);

    while (!section_queue.empty())
    {
        section_node node = section_queue.front();
        section_queue.pop_front();

        // Mark the VBO as visible
        node.sect->visible = true;

        auto visit = [&](vec3i pos, int8_t through)
        {
            NOP_FIX;
            // Don't revisit directions we have already visited
            if (node.dirs[through ^ 1])
                return;

            // Skip if the position is out of bounds
            section *next_section = section_at(pos);
            if (!next_section)
                return;

            // Don't revisit sections we have already visited
            if (std::find(visited.begin(), visited.end(), next_section) != visited.end())
                return;

            if (node.from != -1)
            {
                // Check if the node is visible from the face we entered
                if (!(node.sect->visibility_flags & (face_pair_to_flag(node.from, through))))
                    return;
            }

            vec3f section_offset = vec3f(pos.x + 8, pos.y + 8, pos.z + 8) - fpos;

            // Check if the section is within the render distance
            if (std::abs(section_offset.x) + std::abs(section_offset.z) > RENDER_DISTANCE)
                return;

            if (!is_cube_visible(*frustum, vec3f(pos.x + 8, pos.y + 8, pos.z + 8), 16.0f))
            {
                return;
            }

            // Mark the section as visited
            visited.push_front(next_section);

            // Prepare next node
            section_node new_node;
            new_node.sect = next_section;
            new_node.from = through ^ 1;

            // Copy the directions from the current node
            std::memcpy(new_node.dirs, node.dirs, sizeof(new_node.dirs));

            // Mark the direction we came from as visited
            new_node.dirs[through] = 1;

            // Add the new node to the queue
            section_queue.push_back(new_node);
        };
        vec3i origin = vec3i(node.sect->x, node.sect->y, node.sect->z);
        for (uint8_t i = 0; i < 6; i++)
        {
            visit(origin + (face_offsets[i] * 16), i ^ 1);
        }
    }
}

void world::edit_blocks()
{
    guVector forward = angles_to_vector(xrot, yrot);

    if (!player.selected_item)
        return;

    if (should_place_block && (player.selected_item->empty() || !player.selected_item->as_item().is_block()))
    {
        should_place_block = false;
        should_destroy_block = false;
        return;
    }
    block_t selected_block = block_t{uint8_t(player.selected_item->id & 0xFF), 0x7F, uint8_t(player.selected_item->meta & 0xFF)};
    bool finish_destroying = should_destroy_block && player.block_mine_progress >= 1.0f;

    player.draw_block_outline = raycast_precise(vec3f(player_pos.x, player_pos.y, player_pos.z), vec3f(forward.x, forward.y, forward.z), 4, &player.raycast_pos, &player.raycast_face, player.block_bounds);
    if (player.draw_block_outline)
    {
        BlockID new_blockid = finish_destroying ? BlockID::air : selected_block.get_blockid();
        if (finish_destroying || should_place_block)
        {
            block_t *targeted_block = get_block_at(player.raycast_pos);
            vec3i editable_pos = finish_destroying ? (player.raycast_pos) : (player.raycast_pos + player.raycast_face);
            block_t *editable_block = get_block_at(editable_pos);
            if (editable_block)
            {
                block_t old_block = *editable_block;
                BlockID old_blockid = editable_block->get_blockid();
                BlockID targeted_blockid = targeted_block->get_blockid();
                if (!is_remote())
                {
                    // Handle slab placement
                    if (properties(new_blockid).m_render_type == RenderType::slab)
                    {
                        bool same_as_target = targeted_block->get_blockid() == new_blockid;

                        uint8_t new_meta = player.raycast_face.y == -1 ? 8 : 0;
                        new_meta ^= same_as_target;

                        if (player.raycast_face.y != 0 && (new_meta ^ 8) == (targeted_block->meta & 8) && same_as_target)
                        {
                            targeted_block->set_blockid(BlockID(uint8_t(new_blockid) - 1));
                            targeted_block->meta = 0;
                        }
                        else if (old_blockid == new_blockid)
                        {
                            editable_block->set_blockid(BlockID(uint8_t(new_blockid) - 1));
                            editable_block->meta = 0;
                        }
                        else
                        {
                            editable_block->set_blockid(new_blockid);
                            if (player.raycast_face.y == 0 && properties(targeted_blockid).m_render_type == RenderType::slab)
                                editable_block->meta = targeted_block->meta;
                            else if (player.raycast_face.y == 0)
                                editable_block->meta = new_meta;
                            else
                                editable_block->meta = new_meta ^ same_as_target;
                        }
                    }
                    else
                    {
                        should_place_block &= old_blockid == BlockID::air || properties(old_blockid).m_fluid;
                        if (!finish_destroying && should_place_block)
                        {
                            editable_block->meta = new_blockid == BlockID::air ? 0 : selected_block.meta;
                            editable_block->set_blockid(new_blockid);
                        }
                    }
                    if (finish_destroying)
                    {
                        editable_block->meta = new_blockid == BlockID::air ? 0 : selected_block.meta;
                        editable_block->set_blockid(new_blockid);
                    }
                }

                if (finish_destroying)
                {
                    destroy_block(editable_pos, &old_block);
                    if (is_remote())
                    {
                        // Restore the old block - server will handle the destruction
                        set_block_at(editable_pos, old_blockid);

                        // Send block destruction packet to the server
                        for (uint8_t face_num = 0; face_num < 6; face_num++)
                        {
                            if (player.raycast_face == face_offsets[face_num])
                            {
                                client->sendBlockDig(2, editable_pos.x, editable_pos.y, editable_pos.z, (face_num + 4) % 6);
                                break;
                            }
                        }
                    }
                }
                else if (should_place_block)
                {
                    if (!is_remote())
                    {
                        update_block_at(editable_pos);
                        update_neighbors(editable_pos);
                    }
                    for (uint8_t face_num = 0; face_num < 6; face_num++)
                    {
                        if (player.raycast_face == face_offsets[face_num])
                        {
                            place_block(editable_pos, player.raycast_pos, &selected_block, face_num);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (finish_destroying)
    {
        player.block_mine_progress = 0.0f;
        player.mining_tick = -6;
    }

    // Clear the place/destroy block flags to prevent placing blocks immediately.
    should_place_block = false;
}

int world::prepare_chunks(int count)
{
    const int center_x = (int(std::floor(player.m_entity->position.x)) >> 4);
    const int center_z = (int(std::floor(player.m_entity->position.z)) >> 4);
    const int start_x = center_x - CHUNK_DISTANCE;
    const int start_z = center_z - CHUNK_DISTANCE;

    for (int x = start_x, rx = -CHUNK_DISTANCE; count && rx <= CHUNK_DISTANCE; x++, rx++)
    {
        for (int z = start_z, rz = -CHUNK_DISTANCE; count && rz <= CHUNK_DISTANCE; z++, rz++)
        {
            if (std::abs(rx) + std::abs(rz) > ((CHUNK_DISTANCE * 3) >> 1))
                continue;
            if (add_chunk(x, z))
                count--;
        }
    }
    return count;
}

void world::remove_chunk(chunk_t *chunk)
{
    for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
    {
        section &current = chunk->sections[j];
        current.visible = false;

        if (current.solid && current.solid != current.cached_solid)
        {
            current.solid.clear();
        }
        if (current.transparent && current.transparent != current.cached_transparent)
        {
            current.transparent.clear();
        }

        current.cached_solid.clear();
        current.cached_transparent.clear();
    }
    chunk->generation_stage = ChunkGenStage::invalid;
}

void world::cleanup_chunks()
{
    std::deque<chunk_t *> &chunks = get_chunks();
    chunks.erase(
        std::remove_if(chunks.begin(), chunks.end(),
                       [](chunk_t *&c)
                       {if(!c) return true; if(c->generation_stage == ChunkGenStage::invalid) {delete c; c = nullptr; return true;} return false; }),
        chunks.end());
}

void world::destroy_block(const vec3i pos, block_t *old_block)
{
    BlockID old_blockid = old_block->get_blockid();
    set_block_at(pos, BlockID::air);
    update_block_at(pos);

    // Add block particles
    javaport::Random rng;

    int texture_index = get_face_texture_index(old_block, FACE_NX);

    particle particle;
    particle.max_life_time = 60;
    particle.physics = PPHYSIC_FLAG_ALL;
    particle.type = PTYPE_BLOCK_BREAK;
    particle.size = 8;
    particle.brightness = 0xFF;
    int u = PARTICLE_X(texture_index);
    int v = PARTICLE_Y(texture_index);
    for (int i = 0; i < 64; i++)
    {
        // Randomize the particle position and velocity
        particle.position = vec3f(pos.x, pos.y, pos.z) + vec3f(rng.nextFloat() - .5f, rng.nextFloat() - .5f, rng.nextFloat() - .5f);
        particle.velocity = vec3f(rng.nextFloat() - .5f, rng.nextFloat() - .25f, rng.nextFloat() - .5f) * 7.5;

        // Randomize the particle texture coordinates
        particle.u = u + (rng.next(2) << 2);
        particle.v = v + (rng.next(2) << 2);

        // Randomize the particle life time by up to 10 ticks
        particle.life_time = particle.max_life_time - (rand() % 10);

        add_particle(particle);
    }

    sound sound = get_break_sound(old_blockid);
    sound.volume = 0.4f;
    sound.pitch *= 0.8f;
    sound.position = vec3f(pos.x, pos.y, pos.z);
    play_sound(sound);

    if (is_remote())
        return;

    // Client side block destruction - only for local play
    update_neighbors(pos);
    properties(old_blockid).m_destroy(pos, *old_block);
    spawn_drop(pos, old_block, properties(old_blockid).m_drops(*old_block));
}

void world::place_block(const vec3i pos, const vec3i targeted, block_t *new_block, uint8_t face)
{
    sound sound = get_mine_sound(new_block->get_blockid());
    sound.volume = 0.4f;
    sound.pitch *= 0.8f;
    sound.position = vec3f(pos.x, pos.y, pos.z);
    m_sound_system.play_sound(sound);
    player.m_inventory[player.selected_hotbar_slot].count--;
    if (player.m_inventory[player.selected_hotbar_slot].count == 0)
        player.m_inventory[player.selected_hotbar_slot] = inventory::item_stack();

    if (is_remote())
        client->sendPlaceBlock(targeted.x, targeted.y, targeted.z, (face + 4) % 6, new_block->id, 1, new_block->meta);
}

void world::spawn_drop(const vec3i &pos, const block_t *old_block, inventory::item_stack item)
{
    if (item.empty())
        return;
    // Drop items
    javaport::Random rng;
    vec3f item_pos = vec3f(pos.x, pos.y, pos.z) + vec3f(0.5);
    entity_item *entity = new entity_item(item_pos, item);
    entity->ticks_existed = 10; // Halves the pickup delay (20 ticks / 2 = 10)
    entity->velocity = vec3f(rng.nextFloat() - .5f, rng.nextFloat(), rng.nextFloat() - .5f) * 0.25f;
    add_entity(entity);
}

void world::create_explosion(vec3f pos, float power, chunk_t *near)
{
    javaport::Random rng;

    sound sound = get_sound("random/old_explode");
    sound.position = pos;
    sound.volume = 0.5;
    sound.pitch = 0.8;
    m_sound_system.play_sound(sound);

    particle particle;
    particle.max_life_time = 80;
    particle.physics = PPHYSIC_FLAG_COLLIDE;
    particle.type = PTYPE_TINY_SMOKE;
    particle.brightness = 0xFF;
    particle.velocity = vec3f(0, 0.5, 0);
    particle.a = 0xFF;
    for (int i = 0; i < 64; i++)
    {
        // Randomize the particle position and velocity
        particle.position = pos + vec3f(rng.nextFloat() - .5f, rng.nextFloat() - .5f, rng.nextFloat() - .5f) * power * 2;

        // Randomize the particle life time by up to 10 ticks
        particle.life_time = particle.max_life_time - (rng.nextInt(20)) - 20;

        // Randomize the particle size
        particle.size = rand() % 64 + 64;

        // Randomize the particle color
        particle.r = particle.g = particle.b = rand() % 63 + 192;

        m_particle_system.add_particle(particle);
    }
    if (!is_remote())
        explode(pos, power * 0.75f, near);
}

void world::draw(camera_t &camera)
{
    // Enable backface culling for terrain
    GX_SetCullMode(GX_CULL_BACK);

    // Prepare the transformation matrix
    transform_view(gertex::get_view_matrix(), vec3f(0.5));

    // Prepare opaque rendering parameters
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    gertex::set_blending(gertex::GXBlendMode::overwrite);
    GX_SetAlphaUpdate(GX_TRUE);

    // Draw particles
    gertex::set_alpha_cutoff(1);
    draw_particles(camera, m_particle_system.particles, m_particle_system.size());

    // Draw chunks
    gertex::set_alpha_cutoff(0);
    draw_scene(true);

    // Prepare transparent rendering parameters
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    gertex::set_blending(gertex::GXBlendMode::normal);
    GX_SetAlphaUpdate(GX_FALSE);

    // Draw chunks
    gertex::set_alpha_cutoff(1);
    draw_scene(false);
}

void world::draw_scene(bool opaque)
{
    std::deque<chunk_t *> &chunks = get_chunks();
    // Use terrain texture
    use_texture(terrain_texture);

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    std::deque<std::pair<section *, vbo_buffer_t *>> sections_to_draw;

    // Draw the solid pass
    if (opaque)
    {
        for (chunk_t *&chunk : chunks)
        {
            if (chunk && chunk->generation_stage == ChunkGenStage::done && chunk->lit_state)
            {
                for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
                {
                    section &current = chunk->sections[j];
                    if (!current.visible || !current.cached_solid.buffer || !current.cached_solid.length)
                        continue;
                    sections_to_draw.push_back(std::make_pair(&current, &current.cached_solid));
                }
            }
        }
    }
    // Draw the transparent pass
    else
    {
        for (chunk_t *&chunk : chunks)
        {
            if (chunk && chunk->generation_stage == ChunkGenStage::done && chunk->lit_state)
            {
                for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
                {
                    section &current = chunk->sections[j];
                    if (!current.visible || !current.cached_transparent.buffer || !current.cached_transparent.length)
                        continue;
                    sections_to_draw.push_back(std::make_pair(&current, &current.cached_transparent));
                }
            }
        }
    }

    // Draw the vbos
    for (std::pair<section *, vbo_buffer_t *> &pair : sections_to_draw)
    {
        section *&sect = pair.first;
        vbo_buffer_t *&buffer = pair.second;
        transform_view(gertex::get_view_matrix(), vec3f(sect->x, sect->y, sect->z) + vec3f(0.5));
        GX_CallDispList(buffer->buffer, buffer->length);
    }

    for (chunk_t *&chunk : chunks)
    {
        chunk->render_entities(partial_ticks, !opaque);
    }

    if (player.draw_block_outline && should_destroy_block && player.mining_tick > 0)
    {
        chunk_t *targeted_chunk = get_chunk_from_pos(player.raycast_pos);
        block_t *targeted_block = targeted_chunk ? targeted_chunk->get_block(player.raycast_pos) : nullptr;
        if (targeted_block && targeted_block->get_blockid() != BlockID::air)
        {
            // Create a copy of the targeted block for rendering
            block_t targeted_block_copy = *targeted_block;

            // Set light to maximum for rendering
            targeted_block_copy.light = 0xFF;

            // Save the current state
            gertex::GXState state = gertex::get_state();

            // Use blending mode for block breaking animation
            gertex::set_blending(gertex::GXBlendMode::multiply2);

            // Apply the transformation for the block breaking animation
            vec3f adjusted_offset = vec3f(player.raycast_pos.x, player.raycast_pos.y, player.raycast_pos.z) + vec3f(0.5);
            vec3f towards_camera = vec3f(player_pos) - adjusted_offset;
            towards_camera.fast_normalize();
            towards_camera = towards_camera * 0.002;

            transform_view(gertex::get_view_matrix(), adjusted_offset + towards_camera);

            // Override texture index for the block breaking animation
            override_texture_index(240 + (int)(player.block_mine_progress * 10.0f));

            render_single_block(targeted_block_copy, !opaque);

            // Reset texture index override
            override_texture_index(-1);

            // Restore the previous state
            gertex::set_state(state);
        }
    }

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw block outlines
    if (!opaque && player.draw_block_outline)
    {
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        vec3f outline_pos = player.raycast_pos;

        vec3f towards_camera = vec3f(player_pos) - outline_pos;
        towards_camera.fast_normalize();
        towards_camera = towards_camera * 0.002;

        vec3f b_min = vec3f(player.raycast_pos.x, player.raycast_pos.y, player.raycast_pos.z) + vec3f(1.0);
        vec3f b_max = vec3f(player.raycast_pos.x, player.raycast_pos.y, player.raycast_pos.z);
        for (aabb_t &bounds : player.block_bounds)
        {
            b_min.x = std::min(bounds.min.x, b_min.x);
            b_min.y = std::min(bounds.min.y, b_min.y);
            b_min.z = std::min(bounds.min.z, b_min.z);

            b_max.x = std::max(bounds.max.x, b_max.x);
            b_max.y = std::max(bounds.max.y, b_max.y);
            b_max.z = std::max(bounds.max.z, b_max.z);
        }
        vec3f floor_b_min = vec3f(std::floor(b_min.x), std::floor(b_min.y), std::floor(b_min.z));

        aabb_t block_outer_bounds;
        block_outer_bounds.min = b_min - floor_b_min;
        block_outer_bounds.max = b_max - floor_b_min;

        transform_view(gertex::get_view_matrix(), floor_b_min + towards_camera);

        // Draw the block outline
        draw_bounds(&block_outer_bounds);
    }
}

void world::draw_selected_block()
{
    player.selected_item = &player.m_inventory[player.selected_hotbar_slot];
    if (get_chunks().size() == 0 || !player.selected_item || player.selected_item->empty())
        return;

    uint8_t light_value = 0;
    // Get the block at the player's position
    block_t *view_block = get_block_at(current_world->player.m_entity->get_foot_blockpos());
    if (view_block)
    {
        // Set the light level of the selected block
        light_value = view_block->light;
    }
    else
    {
        light_value = 0xFF;
    }

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Specify the selected block offset
    vec3f selectedBlockPos = vec3f(+.625f, -.75f, -.75f) + vec3f(-player.view_bob_screen_offset.x, player.view_bob_screen_offset.y, 0);

    int texture_index;
    char *texbuf;

    // Check if the selected item is a block
    if (player.selected_item->as_item().is_block())
    {
        block_t selected_block = block_t{uint8_t(player.selected_item->id & 0xFF), 0x7F, uint8_t(player.selected_item->meta & 0xFF)};
        selected_block.light = light_value;
        RenderType render_type = properties(selected_block.id).m_render_type;

        if (!properties(selected_block.id).m_fluid && (properties(selected_block.id).m_nonflat || render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
        {
            // Render as a block

            // Transform the selected block position
            transform_view_screen(gertex::get_view_matrix(), selectedBlockPos, guVector{.5f, .5f, .5f}, guVector{10, -45, 0});

            // Opaque pass
            GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
            render_single_block(selected_block, false);

            // Transparent pass
            GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_TRUE);
            render_single_block(selected_block, true);
            return;
        }

        // Setup flat item properties

        // Get the texture index of the selected block
        texture_index = get_default_texture_index(BlockID(player.selected_item->id));

        // Use the terrain texture
        use_texture(terrain_texture);
        texbuf = (char *)MEM_PHYSICAL_TO_K0(GX_GetTexObjData(&terrain_texture));
    }
    else
    {
        // Setup item properties

        // Get the texture index of the selected item
        texture_index = player.selected_item->as_item().texture_index;

        // Use the item texture
        use_texture(items_texture);
        texbuf = (char *)MEM_PHYSICAL_TO_K0(GX_GetTexObjData(&items_texture));
    }

    // Render as an item

    // Transform the selected block position
    transform_view_screen(gertex::get_view_matrix(), selectedBlockPos, guVector{.75f, .75f, .75f}, guVector{10, 45, 180});

    uint16_t tex_x = PARTICLE_X(texture_index);
    uint16_t tex_y = PARTICLE_Y(texture_index);

    // Opaque pass - items are always drawn in the opaque pass
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);

    constexpr int tex_width = 256;

    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
        {
            int u = tex_x + x + 1;
            int v = tex_y + 15 - y;

            // Get the index to the 4x4 texel in the target texture
            int index = (tex_width << 2) * (v & ~3) + ((u & ~3) << 4);
            // Put the data within the 4x4 texel into the target texture
            int index_within = ((u & 3) + ((v & 3) << 2)) << 1;

            int next_x = index + index_within;

            u = tex_x + x;
            v = tex_y + 15 - y - 1;

            // Get the index to the 4x4 texel in the target texture
            index = (tex_width << 2) * (v & ~3) + ((u & ~3) << 4);
            // Put the data within the 4x4 texel into the target texture
            index_within = ((u & 3) + ((v & 3) << 2)) << 1;

            int next_y = index + index_within;

            // Check if the texel is transparent
            render_item_pixel(texture_index, x, 15 - y, x == 15 || !texbuf[next_x], y == 15 || !texbuf[next_y], light_value);
        }
}

void world::draw_bounds(aabb_t *bounds)
{
    // Save the current state
    gertex::GXState state = gertex::get_state();

    // Use blending mode for block breaking animation
    gertex::set_blending(gertex::GXBlendMode::multiply2);

    GX_BeginGroup(GX_LINES, 24);

    aabb_t aabb = *bounds;
    vec3f min = aabb.min;
    vec3f size = aabb.max - aabb.min;

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(vertex_property_t(min + vec3f(size.x * k, size.y * i, size.z * j), 0, 0, 127, 127, 127));
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(vertex_property_t(min + vec3f(size.x * i, size.y * k, size.z * j), 0, 0, 127, 127, 127));
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(vertex_property_t(min + vec3f(size.x * i, size.y * j, size.z * k), 0, 0, 127, 127, 127));
            }
        }
    }
    GX_EndGroup();

    // Restore the previous state
    gertex::set_state(state);
}

void world::save()
{
    // Save the world to disk if in singleplayer
    if (!is_remote())
    {
        try
        {
            for (chunk_t *c : get_chunks())
                c->write();
        }
        catch (std::runtime_error &e)
        {
            printf("Failed to save chunk: %s\n", e.what());
        }
        NBTTagCompound level;
        NBTTagCompound *level_data = (NBTTagCompound *)level.setTag("Data", new NBTTagCompound());
        player.m_entity->serialize((NBTTagCompound *)level_data->setTag("Player", new NBTTagCompound));
        level_data->setTag("Time", new NBTTagLong(ticks));
        level_data->setTag("SpawnX", new NBTTagInt(0));
        level_data->setTag("SpawnY", new NBTTagInt(skycast(vec3i(0, 0, 0), nullptr)));
        level_data->setTag("SpawnZ", new NBTTagInt(0));
        level_data->setTag("LastPlayed", new NBTTagLong(time(nullptr) * 1000LL));
        level_data->setTag("LevelName", new NBTTagString("Wii World"));
        level_data->setTag("RandomSeed", new NBTTagLong(seed));
        level_data->setTag("version", new NBTTagInt(19132));

        std::ofstream file("level.dat", std::ios::binary);
        if (file.is_open())
        {
            NBTBase::writeGZip(file, &level);
            file.flush();
            file.close();
        }
    }
}

bool world::load()
{
    // Load the world from disk if in singleplayer
    if (is_remote())
        return false;
    std::ifstream file("level.dat", std::ios::binary);
    if (!file.is_open())
        return false;
    uint32_t file_size = file.seekg(0, std::ios::end).tellg();
    file.seekg(0, std::ios::beg);
    NBTTagCompound *level = nullptr;
    try
    {
        level = NBTBase::readGZip(file, file_size);
    }
    catch (std::runtime_error &e)
    {
        printf("Failed to load level.dat: %s\n", e.what());
    }
    file.close();

    if (!level)
        return false;

    NBTTagCompound *level_data = level->getCompound("Data");
    if (!level_data)
    {
        delete level;
        return false;
    }

    int32_t version = level_data->getInt("version");
    if (version != 19132)
    {
        printf("Unsupported level.dat version: %d\n", version);
        delete level;
        return false;
    }

    // For now, these are the only values we care about
    ticks = level_data->getLong("Time");
    last_entity_tick = ticks;
    last_fluid_tick = ticks;
    time_of_day = ticks % 24000;
    seed = level_data->getLong("RandomSeed");

    // Stop the chunk manager
    deinit_chunk_manager();

    // Re-initialize the chunk provider
    if (chunk_provider)
        delete chunk_provider;
    chunk_provider = new chunkprovider_overworld(seed);

    // Start the chunk manager using the chunk provider
    init_chunk_manager(chunk_provider);

    printf("Loaded world with seed: %lld\n", seed);

    // Load the player data if it exists
    NBTTagCompound *player_tag = level_data->getCompound("Player");
    if (player_tag)
        player.m_entity->deserialize(player_tag);

    // Clean up
    delete level;

    // Based on the player position, the chunks will be loaded around the player - see chunk.cpp
    return true;
}

void world::create()
{
    // Create a new world
    if (is_remote())
        return;

    // Stop the chunk manager
    deinit_chunk_manager();

    // Create a new world with a random seed
    seed = javaport::Random().nextLong();

    // Re-initialize the chunk provider
    if (chunk_provider)
        delete chunk_provider;
    chunk_provider = new chunkprovider_overworld(seed);

    // Start the chunk manager using the chunk provider
    init_chunk_manager(chunk_provider);
}

void world::reset()
{
    // Stop the chunk manager
    deinit_chunk_manager();

    light_engine::deinit();
    loaded = false;
    time_of_day = 0;
    ticks = 0;
    last_entity_tick = 0;
    last_fluid_tick = 0;
    hell = false;
    set_remote(false);
    if (player.m_entity)
        player.m_entity->chunk = nullptr;
    player.m_inventory.clear();
    for (chunk_t *chunk : get_chunks())
    {
        if (chunk)
            remove_chunk(chunk);
    }
    cleanup_chunks();
    mcr::cleanup();

    // Start the chunk manager without a chunk provider
    light_engine::init();
    init_chunk_manager(nullptr);
}

void world::update_entities()
{

    // Find a chunk for any lingering entities
    std::map<int32_t, entity_physical *> &world_entities = get_entities();
    for (auto &&e : world_entities)
    {
        entity_physical *entity = e.second;
        if (!entity->chunk)
        {
            vec3i int_pos = vec3i(int(std::floor(entity->position.x)), 0, int(std::floor(entity->position.z)));
            entity->chunk = get_chunk_from_pos(int_pos);
            if (entity->chunk && std::find(entity->chunk->entities.begin(), entity->chunk->entities.end(), entity) == entity->chunk->entities.end())
                entity->chunk->entities.push_back(entity);
        }
    }

    if (loaded)
    {
        for (auto &&e : world_entities)
        {
            entity_physical *entity = e.second;

            if (!entity || entity->dead)
                continue;

            // Tick the entity
            entity->tick();

            entity->ticks_existed++;
        }

        // Update the entities of the world
        for (chunk_t *&chunk : get_chunks())
        {
            // Update the entities in the chunk
            chunk->update_entities();
        }

        // Update the player entity
        update_player();
        for (auto &&e : world_entities)
        {
            entity_physical *entity = e.second;
            if (!entity || entity->dead)
                continue;
            // Tick the entity animations
            entity->animate();
        }
    }
}

void world::update_player()
{
    // FIXME: This is a temporary fix for an crash with an unknown cause
#ifdef CLIENT_COLLISION
    if (is_remote())
    {
        // Only resolve collisions for the player entity - the server will handle the rest
        if (player && player->chunk)
        {
            // Resolve collisions with neighboring chunks' entities
            for (auto &&e : get_entities())
            {
                entity_physical *&entity = e.second;

                // Ensure entity is not null
                if (!entity)
                    continue;

                // If the entity doesn't currently belong to a chunk, skip processing it
                if (!entity->chunk)
                    continue;

                // Prevent the player from colliding with itself
                if (entity == player)
                    continue;

                // Dead entities should not be checked for collision
                if (entity->dead)
                    continue;

                // Don't collide with items
                if (entity->type == 1)
                    continue;

                // Check if the entity is close enough (within a chunk) to the player and then resolve collision if it collides with the player
                if (std::abs(entity->chunk->x - player->chunk->x) <= 1 && std::abs(entity->chunk->z - player->chunk->z) <= 1 && player->collides(entity))
                {
                    // Resolve the collision from the player's perspective
                    player->resolve_collision(entity);
                }
            }
        }
    }
#endif
    vec3i block_pos = player.m_entity->get_head_blockpos();
    block_t *block = get_block_at(block_pos);
    if (block && properties(block->id).m_fluid && block_pos.y + 2 - get_fluid_height(block_pos, block->get_blockid(), player.m_entity->chunk) >= player.m_entity->aabb.min.y + player.m_entity->y_offset)
    {
        player.in_fluid = properties(block->id).m_base_fluid;
    }
    else
    {
        player.in_fluid = BlockID::air;
    }

    if (should_destroy_block && player.block_mine_progress < 1.0f && player.draw_block_outline)
    {
        static block_t *last_targeted_block = nullptr;
        block_t *targeted_block = get_block_at(player.raycast_pos);
        if (last_targeted_block != targeted_block)
        {
            // Reset the progress if the targeted block has changed
            player.block_mine_progress = 0.0f;
            last_targeted_block = targeted_block;
        }

        if (targeted_block && targeted_block->get_blockid() != BlockID::air)
        {
            if (is_remote() && player.mining_tick == 0)
            {
                // Send block dig packet to the server
                for (uint8_t face_num = 0; face_num < 6; face_num++)
                {
                    if (player.raycast_face == face_offsets[face_num])
                    {
                        client->sendBlockDig(0, player.raycast_pos.x, player.raycast_pos.y, player.raycast_pos.z, (face_num + 4) % 6);
                        break;
                    }
                }
            }
            player.mining_tick++;

            // A cooldown applies when the player finishes mining a block. Negative values indicate the cooldown is still active.
            if (player.mining_tick >= 0)
            {
                // The player hits the block 5 times per second
                if (player.mining_tick % 4 == 1)
                {
                    // Swing the player's arm - TODO: Add a proper client-side animation
                    if (is_remote())
                    {
                        client->sendAnimation(1);
                    }

                    // Play the mining sound
                    sound sound = get_mine_sound(targeted_block->get_blockid());
                    sound.pitch *= 0.5f;
                    sound.volume = 0.15f;
                    sound.position = vec3f(player.raycast_pos.x, player.raycast_pos.y, player.raycast_pos.z);
                    play_sound(sound);

                    // Add block breaking particles
                    particle particle;
                    particle.max_life_time = 60;
                    particle.physics = PPHYSIC_FLAG_ALL;
                    particle.type = PTYPE_BLOCK_BREAK;
                    particle.size = 8;
                    particle.brightness = 0xFF;
                    int texture_index = get_default_texture_index(targeted_block->get_blockid());
                    int u = PARTICLE_X(texture_index);
                    int v = PARTICLE_Y(texture_index);

                    // Calculate the bounds of the block to spawn particles around
                    vec3f b_min = vec3f(player.raycast_pos.x, player.raycast_pos.y, player.raycast_pos.z) + vec3f(1.0, 1.0, 1.0);
                    vec3f b_max = vec3f(player.raycast_pos.x, player.raycast_pos.y, player.raycast_pos.z);
                    bool hitbox = false;
                    for (aabb_t &bounds : player.block_bounds)
                    {
                        hitbox = true;
                        b_min.x = std::min(bounds.min.x, b_min.x);
                        b_min.y = std::min(bounds.min.y, b_min.y);
                        b_min.z = std::min(bounds.min.z, b_min.z);

                        b_max.x = std::max(bounds.max.x, b_max.x);
                        b_max.y = std::max(bounds.max.y, b_max.y);
                        b_max.z = std::max(bounds.max.z, b_max.z);
                    }
                    aabb_t block_outer_bounds = aabb_t(b_min, b_max);
                    javaport::Random rng;

                    for (int i = 0; hitbox && i < 8; i++)
                    {
                        // Randomize the particle position and velocity
                        vec3f local_pos = vec3f(rng.nextFloat(), rng.nextFloat(), rng.nextFloat());

                        vec3f pos = (block_outer_bounds.min + block_outer_bounds.max) * 0.5 + local_pos;

                        // Put the particle at the edge of the block by rounding the position to the nearest block edge on a random axis
                        // This will ensure that the particles are spawned at the edges of the block
                        int axis_to_round = rng.nextInt(3);
                        if (axis_to_round == 0)
                        {
                            pos.x = std::round(pos.x);
                        }
                        else if (axis_to_round == 1)
                        {
                            pos.y = std::round(pos.y);
                        }
                        else
                        {
                            pos.z = std::round(pos.z);
                        }
                        // Force the particle within the bounds of the block
                        pos.x = std::clamp(pos.x - 0.5, block_outer_bounds.min.x, block_outer_bounds.max.x) - 0.5;
                        pos.y = std::clamp(pos.y - 0.5, block_outer_bounds.min.y, block_outer_bounds.max.y);
                        pos.z = std::clamp(pos.z - 0.5, block_outer_bounds.min.z, block_outer_bounds.max.z) - 0.5;

                        particle.position = pos;

                        // Randomize the particle velocity
                        particle.velocity = vec3f(rng.nextFloat() - .5f, rng.nextFloat() - .25f, rng.nextFloat() - .5f) * 3.0f;

                        // Randomize the particle texture coordinates
                        particle.u = u + (rng.next(2) << 2);
                        particle.v = v + (rng.next(2) << 2);

                        // Randomize the particle life time by up to 10 ticks
                        particle.life_time = particle.max_life_time - (rand() % 10);

                        add_particle(particle);
                    }
                }

                // Increase the mining progress
                player.block_mine_progress += properties(targeted_block->get_blockid()).get_break_multiplier(*player.selected_item, player.m_entity->on_ground, basefluid(player.in_fluid) == BlockID::water);
            }
            else
            {
                player.block_mine_progress = 0.0f;
            }
        }
    }
    else
    {
        player.block_mine_progress = 0.0f;
        if (player.mining_tick < 0)
            player.mining_tick++;
        else
            player.mining_tick = 0;
    }
}

void world::add_particle(const particle &particle)
{
    m_particle_system.add_particle(particle);
}

void world::play_sound(const sound &sound)
{
    m_sound_system.play_sound(sound);
}

player_properties::player_properties()
{
    this->selected_item = &this->m_inventory[selected_hotbar_slot];
}

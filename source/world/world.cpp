#include "world.hpp"

#include <algorithm>
#include <unistd.h>
#include <ported/Random.hpp>
#include <crapper/client.hpp>
#include <filesystem>

#include <util/timers.hpp>
#include <world/chunk.hpp>
#include <block/blocks.hpp>
#include <world/entity.hpp>
#include <render/render.hpp>
#include <world/particle.hpp>
#include <mcregion.hpp>
#include <util/lock.hpp>
#include <world/util/raycast.hpp>
#include <nbt/nbt.hpp>
#include <world/light.hpp>
#include <gui/gui_dirtscreen.hpp>
#include <gui/gui_survival.hpp>
#include <gui/gui_crafting.hpp>
#include <gui/gui_gameover.hpp>
#include <world/chunkprovider.hpp>
#include <registry/tile_entities.hpp>
#include <util/face_pair.hpp>
#include <util/debuglog.hpp>
#include <light_nether_rgba.h>

extern bool should_destroy_block;
extern bool should_place_block;

World::World(std::string name)
    : name(name)
{
    namespace fs = std::filesystem;

    std::string save_path = "/apps/minecraft/saves/" + name;
    std::string region_path = save_path + "/region";
    fs::create_directories(region_path);
    fs::current_path(save_path);
    LightEngine::init(this);
}

World::World()
{
    LightEngine::init(this);
}

World::~World()
{
    LightEngine::deinit();
    mcr::cleanup();
    deinit_chunk_manager();
    set_remote(false);
    if (tick_mutex != LWP_MUTEX_NULL)
        LWP_MutexDestroy(tick_mutex);
    if (chunk_mutex != LWP_MUTEX_NULL)
        LWP_MutexDestroy(chunk_mutex);

    if (chunk_provider)
        delete chunk_provider;
    if (frustum)
        delete frustum;
}

bool World::is_remote()
{
    return client != nullptr;
}

void World::set_remote(bool value)
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

bool World::tick()
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

            // Inform the user about the network error
            GuiDirtscreen *dirtscreen = new GuiDirtscreen;
            dirtscreen->set_text(std::string(e.what()));
            Gui::set_gui(dirtscreen);

            return false;
        }
    }
    update_entities();
    calculate_visibility();
    update_chunks();

    // Calculate chunk memory usage
    memory_usage = 0;
    for (Chunk *&chunk : chunks)
    {
        memory_usage += chunk ? chunk->size() : 0;
    }

    if (m_sound_system)
        m_sound_system->update(angles_to_vector(0, get_camera().rot.y + 90), player.get_position(std::fmod(partial_ticks, 1)), true);

    last_entity_tick = ticks;

    if (player.dead && loaded)
    {
        player.velocity = Vec3f(0, 0, 0);
        player.prev_position = player.position;
        GuiGameOver *game_over = dynamic_cast<GuiGameOver *>(Gui::get_gui());
        if (!game_over)
        {
            GuiGameOver *game_over = new GuiGameOver(this);
            Gui::set_gui(game_over);

            // Drop all items
            for (size_t i = 0; i < player.items.size(); i++)
            {
                if (!player.items[i].empty())
                {
                    EntityItem *item = new EntityItem(player.get_position(0) + Vec3f(0, 1, 0), player.items[i]);
                    javaport::Random rng;
                    item->velocity = Vec3f((rng.nextFloat() - 0.5f), rng.nextFloat() * 0.1f + 0.1f, (rng.nextFloat() - 0.5f)) * 0.1f;
                    add_entity(item);
                }
            }
            player.items.clear();
        }
        else
        {
            if (game_over->quitting)
                return false;
        }
    }

    return true;
}

void World::update()
{
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

void World::update_frustum(Camera &camera)
{
    if (!frustum)
        frustum = new Frustum;
    build_frustum(camera, *frustum);
}

void World::update_chunks()
{
    int light_up_calls = 0;
    std::vector<Chunk *> to_remove;
    for (Chunk *&chunk : chunks)
    {
        if (chunk)
        {
            int hdistance = chunk->player_taxicab_distance();
            if (hdistance > CHUNK_DISTANCE * 24 + 24 && !is_remote())
            {
                to_remove.push_back(chunk);
                continue;
            }
            if (chunk->state != ChunkState::done)
                continue;

            if (!chunk->lit_state && light_up_calls < 5)
            {
                light_up_calls++;
                chunk->light_up();
            }
        }
    }
    for (Chunk *&chunk : to_remove)
    {
        save_and_clean_chunk(chunk);
    }
    if (!is_remote())
    {
        tick_blocks();
        prepare_chunks(1);
    }
}

void World::tick_blocks()
{
    Lock lock(tick_mutex);
    while (!scheduled_updates.empty())
    {
        BlockTick block_tick = *scheduled_updates.begin();
        if ((block_tick.ticks & 0x7FFFFFFF) > this->ticks)
            break;
        Block *block = get_block_at(block_tick.pos);
        if (block && block->get_blockid() == block_tick.block_id)
        {
            BlockProperties &props = properties(block->id);
            if (props.m_tick)
            {
                props.m_tick(this, block_tick.pos, *block);
            }
        }
        scheduled_updates.erase(block_tick);
    }
}

void World::schedule_block_update(const Vec3i &pos, BlockID id, int ticks)
{
    if (is_remote())
        return;

    BlockTick block_tick(id, pos, this->ticks + std::max(0, ticks));
    if (scheduled_updates.find(block_tick) != scheduled_updates.end())
        return;
    scheduled_updates.insert(block_tick);
}

SectionUpdatePhase World::update_sections(SectionUpdatePhase phase)
{
    // Buffer to hold VBOs that need to be flushed
    static std::vector<Section *> vbos_to_flush;

    // Pointer to the current VBO being processed
    static Section *curr_section = nullptr;

    // Gets the VBO at the given position
    auto section_at = [this](const Vec3i &pos) -> Section *
    {
        // Out of bounds check for Y coordinate
        if (pos.y < 0 || pos.y >= WORLD_HEIGHT)
            return nullptr;

        // Get the chunk from the position
        Chunk *chunk = get_chunk_from_pos(pos);

        // If the chunk doesn't exist, neither does the VBO
        if (!chunk)
            return nullptr;

        return &chunk->sections[pos.y >> 4];
    };

    // Process the current VBO based on the phase
    switch (phase)
    {
    case SectionUpdatePhase::BLOCK_VISIBILITY:
    {
        for (Chunk *&chunk : chunks)
        {
            if (chunk && chunk->state == ChunkState::done && !chunk->light_update_count)
            {
                // Check if chunk has other chunks around it.
                bool surrounding = true;
                for (int i = 0; i < 6; i++)
                {
                    // Skip the top and bottom faces
                    if (i == 2)
                        i = 4;
                    // Check if the surrounding chunk exists and has no lighting updates pending
                    Chunk *surrounding_chunk = get_chunk(chunk->x + face_offsets[i].x, chunk->z + face_offsets[i].z);
                    if (!surrounding_chunk || surrounding_chunk->light_update_count || surrounding_chunk->state != ChunkState::done)
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
                    Section &other_section = chunk->sections[j];
                    if (other_section.visible && other_section.dirty && (!curr_section || *other_section.chunk < *curr_section->chunk))
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
    case SectionUpdatePhase::SECTION_VISIBILITY:
        if (!curr_section)
            break;
        curr_section->chunk->refresh_section_visibility(curr_section->y >> 4);
        break;
    case SectionUpdatePhase::SOLID:
        if (!curr_section)
            break;
        curr_section->chunk->build_vbo(curr_section->y >> 4, false);
        break;
    case SectionUpdatePhase::TRANSPARENT:
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
    case SectionUpdatePhase::FLUSH:
    {
        // Flush cached buffers
        std::vector<Section *> skipped_sections;
        for (Section *current : vbos_to_flush)
        {
            if (current->dirty)
                continue;
            if (sync_chunk_updates)
            {
                bool updated_neighbors = true;
                for (int i = 0; i < 6; i++)
                {
                    Vec3i neighbor_pos = Vec3i(current->x, current->y, current->z) + face_offsets[i] * 16;
                    Section *neighbor = section_at(neighbor_pos);
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
        for (Section *skipped : skipped_sections)
        {
            vbos_to_flush.push_back(skipped);
        }
        break;
    }
    default:
        break;
    }
    if (phase++ != SectionUpdatePhase::FLUSH && !curr_section)
    {
        // Skip to flush phase if no section is set
        phase = SectionUpdatePhase::FLUSH;
    }

    return phase;
}

/**
 * This implementation of cave culling is based on the documentation
 * and the implementation of the original Minecraft culling algorithm.
 * You can find the original 2D implementation here:
 * https://tomcc.github.io/index.html
 */

struct SectionNode
{
    Section *sect;
    int8_t from;     // The face we entered the VBO from
    uint8_t dirs[6]; // For preventing revisits of the same face
};

void World::calculate_visibility()
{
    init_face_pairs();
    // Reset visibility status for all VBOs
    for (Chunk *&chunk : chunks)
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
    auto section_at = [this](const Vec3i &pos) -> Section *
    {
        // Out of bounds check for Y coordinate
        if (pos.y < 0 || pos.y >= WORLD_HEIGHT)
            return nullptr;

        // Get the chunk from the position
        Chunk *chunk = get_chunk_from_pos(pos);

        // If the chunk doesn't exist, neither does the VBO
        if (!chunk)
            return nullptr;

        return &chunk->sections[pos.y >> 4];
    };

    std::deque<SectionNode> section_queue;
    std::deque<Section *> visited;
    Vec3f fpos = get_camera().position;
    SectionNode entry;
    entry.sect = section_at(Vec3i(int(fpos.x), int(fpos.y), int(fpos.z)));

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
        SectionNode node = section_queue.front();
        section_queue.pop_front();

        // Mark the VBO as visible
        node.sect->visible = true;

        auto visit = [&](Vec3i pos, int8_t through)
        {
            // Don't revisit directions we have already visited
            if (node.dirs[through ^ 1])
                return;

            // Skip if the position is out of bounds
            Section *next_section = section_at(pos);
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

            Vec3f section_offset = Vec3f(pos.x + 8, pos.y + 8, pos.z + 8) - fpos;

            // Check if the section is within the render distance
            if (std::abs(section_offset.x) + std::abs(section_offset.z) > RENDER_DISTANCE)
                return;

            if (!is_cube_visible(*frustum, Vec3f(pos.x + 8, pos.y + 8, pos.z + 8), 16.0f))
            {
                return;
            }

            // Mark the section as visited
            visited.push_front(next_section);

            // Prepare next node
            SectionNode new_node;
            new_node.sect = next_section;
            new_node.from = through ^ 1;

            // Copy the directions from the current node
            std::memcpy(new_node.dirs, node.dirs, sizeof(new_node.dirs));

            // Mark the direction we came from as visited
            new_node.dirs[through] = 1;

            // Add the new node to the queue
            section_queue.push_back(new_node);
        };
        Vec3i origin = Vec3i(node.sect->x, node.sect->y, node.sect->z);
        for (uint8_t i = 0; i < 6; i++)
        {
            visit(origin + (face_offsets[i] * 16), i ^ 1);
        }
    }
}

void World::edit_blocks()
{
    Camera &camera = get_camera();

    if (!player.selected_item)
        return;

    Block held_block = player.selected_item->as_item().is_block() ? Block{uint8_t(player.selected_item->id & 0xFF), 0x7F, uint8_t(player.selected_item->meta & 0xFF)} : Block{};
    bool finish_destroying = should_destroy_block && player.mining_progress >= 1.0f;

    player.raycast_target_found = raycast_precise(camera.position, angles_to_vector(camera.rot.x, camera.rot.y), 4, &player.raycast_target_pos, &player.raycast_target_face, player.raycast_target_bounds, this);
    if (player.raycast_target_found && (finish_destroying || should_place_block))
    {
        BlockID new_blockid = finish_destroying ? BlockID::air : held_block.get_blockid();

        // The block that the result of the action will be stored in temporarily.
        Block result_block = held_block;

        // If destroying, the result block is air.
        if (finish_destroying)
        {
            result_block.set_blockid(BlockID::air);
            result_block.meta = 0;
        }

        // The position of the targeted block.
        Vec3i result_pos = player.raycast_target_pos;

        // If placing a block, the result position is the block at the face we are targeting.
        if (!finish_destroying)
            result_pos = result_pos + player.raycast_target_face;

        Block *editable_block = get_block_at(result_pos);
        if (editable_block)
        {
            if (!is_remote())
            {
                // Handle slab corner cases

                Block *targeted_block = get_block_at(player.raycast_target_pos);
                if (player.raycast_target_face.y == 1 && new_blockid == BlockID::stone_slab && targeted_block->get_blockid() == BlockID::stone_slab && held_block.meta == targeted_block->meta)
                {
                    // Adjust position to the block being targeted if we're facing the top face of a bottom slab
                    result_pos = player.raycast_target_pos;
                    editable_block = targeted_block;
                }

                // Conditions for turning a bottom slab into a double slab:
                // 1. Both the held block and the editable block are slabs of the same type.
                // 2. The block being placed is on the same vertical level as the targeted block.
                // This prevents the off-by-one issue when placing the top half between two bottom slabs.
                if (new_blockid == BlockID::stone_slab && editable_block->get_blockid() == BlockID::stone_slab && held_block.meta == editable_block->meta && (result_pos.y - player.raycast_target_pos.y) == 0)
                {
                    // Turn bottom slab into double slab
                    result_block.set_blockid(BlockID::double_stone_slab);
                }
                else if (editable_block->get_blockid() != BlockID::air && !properties(editable_block->id).m_fluid)
                {
                    should_place_block = false;
                }

                // Block use handlers
                if (!finish_destroying && should_place_block)
                {
                    switch (targeted_block->get_blockid())
                    {
                    case BlockID::crafting_table:
                    {
                        Gui::set_gui(new GuiCrafting(&player, nullptr, 0));
                        should_place_block = false;
                        break;
                    }
                    case BlockID::chest:
                    {
                        TileEntity *tile_entity = get_tile_entity(player.raycast_target_pos);

                        // The loaded tile entity could technically be any tile entity.
                        // Who knows if this chest has the data of a furnace?
                        TileEntityChest *chest = dynamic_cast<TileEntityChest *>(tile_entity);
                        if (chest)
                        {
                            Gui::set_gui(new GuiContainer(&player, &chest->items));
                        }
                        should_place_block = false;
                        break;
                    }
                    default:
                        break;
                    }
                }
            }

            if (finish_destroying)
            {
                // For restoring the state

                Block old_editable_block = *editable_block;

                destroy_block(result_pos, &old_editable_block);
                if (is_remote())
                {
                    // Restore the old block as the server will handle the destruction.
                    // This is required to prevent desync caused by spawn protection.
                    *editable_block = old_editable_block;

                    // Send block destruction packet to the server
                    for (uint8_t face_num = 0; face_num < 6; face_num++)
                    {
                        if (player.raycast_target_face == face_offsets[face_num])
                        {
                            client->sendBlockDig(2, result_pos.x, result_pos.y, result_pos.z, (face_num + 4) % 6);
                            break;
                        }
                    }
                }
            }
            else if (should_place_block)
            {
                // Look up the face number we are targeting
                uint8_t face_num = 0;
                for (face_num = 0; face_num < 6; face_num++)
                {
                    if (player.raycast_target_face == face_offsets[face_num])
                        break;
                }
                // Try to place the block
                bool success = place_block(result_pos, player.raycast_target_pos, &held_block, face_num);
                if (success)
                {
                    if (result_block.get_blockid() != BlockID::air)
                    {
                        set_block_and_meta_at(result_pos, result_block.get_blockid(), result_block.meta);
                    }
                    if (!is_remote())
                    {
                        notify_at(result_pos);
                    }
                }
            }
        }
    }

    if (finish_destroying)
    {
        player.mining_progress = 0.0f;
        player.mining_tick = -6;
    }

    // Clear the place/destroy block flags to prevent placing blocks immediately.
    should_place_block = false;
}

int World::prepare_chunks(int count)
{
    const int center_x = (int(std::floor(player.position.x)) >> 4);
    const int center_z = (int(std::floor(player.position.z)) >> 4);
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

void World::save_and_clean_chunk(Chunk *chunk)
{
    for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
    {
        Section &current = chunk->sections[j];
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
    save_chunk(chunk);
}

void World::destroy_block(const Vec3i pos, Block *old_block)
{
    BlockID old_blockid = old_block->get_blockid();
    set_block_at(pos, BlockID::air);
    notify_at(pos);

    // Add block particles
    javaport::Random rng;

    int texture_index = get_face_texture_index(old_block, FACE_NX);

    Particle particle;
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
        particle.position = Vec3f(pos.x, pos.y, pos.z) + Vec3f(rng.nextFloat() - .5f, rng.nextFloat() - .5f, rng.nextFloat() - .5f);
        particle.velocity = Vec3f(rng.nextFloat() - .5f, rng.nextFloat() - .25f, rng.nextFloat() - .5f) * 7.5;

        // Randomize the particle texture coordinates
        particle.u = u + (rng.next(2) << 2);
        particle.v = v + (rng.next(2) << 2);

        // Randomize the particle life time by up to 10 ticks
        particle.life_time = particle.max_life_time - (rand() % 10);

        add_particle(particle);
    }

    Sound sound = get_break_sound(old_blockid);
    sound.volume = 0.4f;
    sound.pitch *= 0.8f;
    sound.position = Vec3f(pos.x, pos.y, pos.z);
    play_sound(sound);

    if (is_remote())
        return;

    // Client side block destruction - only for local play
    properties(old_blockid).m_destroy(this, pos, *old_block);
    if (properties(old_blockid).can_be_broken_with(*player.selected_item))
        spawn_drop(pos, old_block, properties(old_blockid).m_drops(*old_block));
}

bool World::place_block(const Vec3i pos, const Vec3i targeted, Block *new_block, uint8_t face)
{

    // Check if the block has collision
    bool intersects_entity = false;
    if (properties(new_block->get_blockid()).m_solid)
    {
        // Ensure no entities are in the way
        for (int x = -1; !intersects_entity && x <= 1; x++)
        {
            for (int z = -1; !intersects_entity && z <= 1; z++)
            {
                Chunk *chunk = get_chunk_from_pos(pos + Vec3i(x, 0, z));
                if (!chunk)
                    continue;
                for (EntityPhysical *&entity : chunk->entities)
                {
                    // Blocks can be placed on items. For other entities, check for intersection.
                    if (dynamic_cast<EntityItem *>(entity) == nullptr && new_block->intersects(entity->aabb, pos))
                    {
                        intersects_entity = true;
                        break;
                    }
                }
            }
        }
    }

    if (!intersects_entity && new_block->id != BlockID::air)
    {
        Sound sound = get_mine_sound(new_block->get_blockid());
        sound.volume = 0.4f;
        sound.pitch *= 0.8f;
        sound.position = Vec3f(pos.x, pos.y, pos.z);
        play_sound(sound);
        player.items[player.selected_hotbar_slot + GuiSurvival::hotbar_start].count--;
        if (player.items[player.selected_hotbar_slot + GuiSurvival::hotbar_start].count == 0)
            player.items[player.selected_hotbar_slot + GuiSurvival::hotbar_start] = item::ItemStack();
    }

    if (is_remote())
        client->sendPlaceBlock(targeted.x, targeted.y, targeted.z, (face + 4) % 6, new_block->id, 1, new_block->meta);

    return !intersects_entity;
}

void World::spawn_drop(const Vec3i &pos, const Block *old_block, item::ItemStack item)
{
    if (item.empty())
        return;
    // Drop items
    javaport::Random rng;
    Vec3f item_pos = Vec3f(pos.x, pos.y, pos.z) + Vec3f(0.5);
    EntityItem *entity = new EntityItem(item_pos, item);
    entity->ticks_existed = 10; // Halves the pickup delay (20 ticks / 2 = 10)
    entity->velocity = Vec3f(rng.nextFloat() - .5f, rng.nextFloat(), rng.nextFloat() - .5f) * 0.25f;
    add_entity(entity);
}

void World::create_explosion(Vec3f pos, float power)
{
    javaport::Random rng;

    Sound sound = get_sound("random/old_explode");
    sound.position = pos;
    sound.volume = 0.5;
    sound.pitch = 0.8;
    play_sound(sound);

    Particle particle;
    particle.max_life_time = 80;
    particle.physics = PPHYSIC_FLAG_COLLIDE;
    particle.type = PTYPE_TINY_SMOKE;
    particle.brightness = 0xFF;
    particle.velocity = Vec3f(0, 0.5, 0);
    particle.a = 0xFF;
    for (int i = 0; i < 64; i++)
    {
        // Randomize the particle position and velocity
        particle.position = pos + Vec3f(rng.nextFloat() - .5f, rng.nextFloat() - .5f, rng.nextFloat() - .5f) * power * 2;

        // Randomize the particle life time by up to 10 ticks
        particle.life_time = particle.max_life_time - (rng.nextInt(20)) - 20;

        // Randomize the particle size
        particle.size = rand() % 64 + 64;

        // Randomize the particle color
        particle.r = particle.g = particle.b = rand() % 63 + 192;

        m_particle_system.add_particle(particle);
    }
    if (!is_remote())
        explode(pos, power * 0.75f, this);
}

void World::draw(Camera &camera)
{
    // Enable backface culling for terrain
    GX_SetCullMode(GX_CULL_BACK);

    // Prepare the transformation matrix
    transform_view(gertex::get_view_matrix(), Vec3f(0.5));

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

void World::draw_scene(bool opaque)
{
    // Use terrain texture
    use_texture(terrain_texture);

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    std::deque<std::pair<Section *, VBO *>> sections_to_draw;

    // Draw the solid pass
    if (opaque)
    {
        for (Chunk *&chunk : chunks)
        {
            if (chunk && chunk->state == ChunkState::done && chunk->lit_state)
            {
                for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
                {
                    Section &current = chunk->sections[j];
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
        for (Chunk *&chunk : chunks)
        {
            if (chunk && chunk->state == ChunkState::done && chunk->lit_state)
            {
                for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
                {
                    Section &current = chunk->sections[j];
                    if (!current.visible || !current.cached_transparent.buffer || !current.cached_transparent.length)
                        continue;
                    sections_to_draw.push_back(std::make_pair(&current, &current.cached_transparent));
                }
            }
        }
    }

    for (Chunk *&chunk : chunks)
    {
        chunk->render_entities(partial_ticks, !opaque);
    }

    // Draw the vbos
    for (std::pair<Section *, VBO *> &pair : sections_to_draw)
    {
        Section *&sect = pair.first;
        VBO *&buffer = pair.second;
        transform_view(gertex::get_view_matrix(), Vec3f(sect->x, sect->y, sect->z) + Vec3f(0.5));
        GX_CallDispList(buffer->buffer, buffer->length);
    }

    if (player.raycast_target_found && should_destroy_block && player.mining_tick > 0)
    {
        Chunk *targeted_chunk = get_chunk_from_pos(player.raycast_target_pos);
        Block *targeted_block = targeted_chunk ? targeted_chunk->get_block(player.raycast_target_pos) : nullptr;
        if (targeted_block && targeted_block->get_blockid() != BlockID::air)
        {
            // Create a copy of the targeted block for rendering
            Block targeted_block_copy = *targeted_block;

            // Set light to maximum for rendering
            targeted_block_copy.light = 0xFF;

            // Save the current state
            gertex::GXState state = gertex::get_state();

            // Use blending mode for block breaking animation
            gertex::set_blending(gertex::GXBlendMode::multiply2);

            // Apply the transformation for the block breaking animation
            Vec3f adjusted_offset = Vec3f(player.raycast_target_pos.x, player.raycast_target_pos.y, player.raycast_target_pos.z) + Vec3f(0.5);
            Vec3f towards_camera = Vec3f(get_camera().position) - adjusted_offset;
            towards_camera.fast_normalize();
            towards_camera = towards_camera * 0.002;

            transform_view(gertex::get_view_matrix(), adjusted_offset + towards_camera);

            // Override texture index for the block breaking animation
            override_texture_index(240 + (int)(player.mining_progress * 10.0f));

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
    if (!opaque && player.raycast_target_found)
    {
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        Vec3f outline_pos = player.raycast_target_pos;

        Vec3f towards_camera = Vec3f(get_camera().position) - outline_pos;
        towards_camera.fast_normalize();
        towards_camera = towards_camera * 0.002;

        Vec3f b_min = player.raycast_target_bounds.min;
        Vec3f b_max = player.raycast_target_bounds.max;
        Vec3f floor_b_min = Vec3f(std::floor(b_min.x), std::floor(b_min.y), std::floor(b_min.z));

        AABB relative_target_bounds;
        relative_target_bounds.min = b_min - floor_b_min;
        relative_target_bounds.max = b_max - floor_b_min;

        transform_view(gertex::get_view_matrix(), floor_b_min + towards_camera);

        // Draw the block outline
        draw_bounds(&relative_target_bounds);
    }
}

void World::draw_selected_block()
{
    player.selected_item = &player.items[player.selected_hotbar_slot + GuiSurvival::hotbar_start];
    if (chunks.size() == 0 || !player.selected_item || player.selected_item->empty())
        return;

    uint8_t light_value = 0;
    // Get the block at the player's position
    Block *view_block = get_block_at(player.get_foot_blockpos());
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
    Vec3f selectedBlockPos = Vec3f(+.625f, -.75f, -.75f) + Vec3f(-player.view_bob_screen_offset.x, player.view_bob_screen_offset.y, 0);

    int texture_index;
    char *texbuf;

    // Check if the selected item is a block
    if (player.selected_item->as_item().is_block())
    {
        Block selected_block = Block{uint8_t(player.selected_item->id & 0xFF), 0x7F, uint8_t(player.selected_item->meta & 0xFF)};
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
        texture_index = player.selected_item->get_texture_index();

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

void World::draw_bounds(AABB *bounds)
{
    // Save the current state
    gertex::GXState state = gertex::get_state();

    // Use blending mode for block breaking animation
    gertex::set_blending(gertex::GXBlendMode::multiply2);

    GX_BeginGroup(GX_LINES, 24);

    AABB aabb = *bounds;
    Vec3f min = aabb.min;
    Vec3f size = aabb.max - aabb.min;

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(Vertex(min + Vec3f(size.x * k, size.y * i, size.z * j), 0, 0, 127, 127, 127));
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(Vertex(min + Vec3f(size.x * i, size.y * k, size.z * j), 0, 0, 127, 127, 127));
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(Vertex(min + Vec3f(size.x * i, size.y * j, size.z * k), 0, 0, 127, 127, 127));
            }
        }
    }
    GX_EndGroup();

    // Restore the previous state
    gertex::set_state(state);
}

void World::save(Progress *prog)
{
    // Save the world to disk if in singleplayer
    if (!is_remote())
    {
        namespace fs = std::filesystem;
        if (prog)
        {
            prog->progress = 0;
            prog->max_progress = chunks.size();
        }
        std::vector<Chunk *> chunks_to_clean;
        chunks_to_clean.reserve(chunks.size());
        for (Chunk *c : chunks)
            try
            {
                c->write();
                if (prog)
                    prog->progress++;
                chunks_to_clean.push_back(c);
            }
            catch (std::runtime_error &e)
            {
                debug::print("Failed to save chunk: %s\n", e.what());
            }
        for (Chunk *&c : chunks_to_clean)
            save_and_clean_chunk(c);
        int64_t dir_size = 0;

        for (auto &file : fs::recursive_directory_iterator(fs::current_path()))
        {
            if (file.is_regular_file())
                dir_size += fs::file_size(file);
        }

        NBTTagCompound level;
        NBTTagCompound *level_data = (NBTTagCompound *)level.setTag("Data", new NBTTagCompound());
        player.serialize((NBTTagCompound *)level_data->setTag("Player", new NBTTagCompound));
        level_data->setTag("Time", new NBTTagLong(ticks));
        level_data->setTag("SpawnX", new NBTTagInt(spawn_pos.x));
        Chunk *spawn_ch = get_chunk_from_pos(spawn_pos);
        int spawn_y = spawn_ch ? skycast(Vec3i(spawn_pos.x, 0, spawn_pos.z), spawn_ch) : spawn_pos.y;
        level_data->setTag("SpawnY", new NBTTagInt(spawn_y < 0 ? this->spawn_pos.y : spawn_y));
        level_data->setTag("SpawnZ", new NBTTagInt(spawn_pos.z));
        level_data->setTag("LastPlayed", new NBTTagLong(time(nullptr) * 1000LL));
        level_data->setTag("LevelName", new NBTTagString("Wii World"));
        level_data->setTag("RandomSeed", new NBTTagLong(seed));
        level_data->setTag("version", new NBTTagInt(19132));
        level_data->setTag("SizeOnDisk", new NBTTagLong(dir_size));

        std::ofstream file("level.dat", std::ios::binary);
        if (file.is_open())
        {
            NBTBase::writeGZip(file, &level);
            file.flush();
            file.close();
        }
    }
}

bool World::load()
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

    spawn_pos.x = level_data->getInt("SpawnX");
    spawn_pos.y = level_data->getInt("SpawnY");
    if (spawn_pos.y <= 0)
        spawn_pos.y = 80; // Default spawn height if not set
    spawn_pos.z = level_data->getInt("SpawnZ");
    seed = level_data->getLong("RandomSeed");

    // Stop the chunk manager
    deinit_chunk_manager();

    // Re-initialize the chunk provider
    if (chunk_provider)
        delete chunk_provider;
    chunk_provider = new ChunkProviderOverworld(this);

    // Start the chunk manager using the chunk provider
    init_chunk_manager(chunk_provider);

    printf("Loaded world with seed: %lld\n", seed);

    // Load the player data if it exists
    NBTTagCompound *player_tag = level_data->getCompound("Player");
    if (player_tag)
        player.deserialize(player_tag);

    // Clean up
    delete level;

    // Based on the player position, the chunks will be loaded around the player - see chunk.cpp
    return true;
}

void World::create()
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
    chunk_provider = new ChunkProviderOverworld(this);

    // Start the chunk manager using the chunk provider
    init_chunk_manager(chunk_provider);
}

void World::set_sound_system(SoundSystem *sound_system)
{
    m_sound_system = sound_system;
}

void World::update_entities()
{

    // Find a chunk for any lingering entities
    for (auto &&e : world_entities)
    {
        EntityPhysical *entity = e.second;
        if (!entity->chunk)
        {
            Vec3i int_pos = Vec3i(int(std::floor(entity->position.x)), 0, int(std::floor(entity->position.z)));
            entity->chunk = get_chunk_from_pos(int_pos);
            if (entity->chunk && std::find(entity->chunk->entities.begin(), entity->chunk->entities.end(), entity) == entity->chunk->entities.end())
                entity->chunk->entities.push_back(entity);
        }
    }

    if (loaded)
    {
        for (auto &&e : world_entities)
        {
            EntityPhysical *entity = e.second;

            if (!entity || entity->dead)
                continue;

            // Tick the entity
            entity->tick();

            entity->ticks_existed++;
        }

        // Update the entities of the world
        for (Chunk *&chunk : chunks)
        {
            // Update the entities in the chunk
            chunk->update_entities();
        }

        // Update the player entity
        update_player();
        if (is_remote())
        {
            for (auto &&e : world_entities)
            {
                EntityPhysical *entity = e.second;
                if (!entity || entity->dead)
                    continue;
                // Tick the entity animations
                entity->animate();
            }
        }
    }
}

void World::update_player()
{
    // FIXME: This is a temporary fix for an crash with an unknown cause
#ifdef CLIENT_COLLISION
    if (is_remote())
    {
        // Only resolve collisions for the player entity - the server will handle the rest
        if (player && player.chunk)
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
                if (std::abs(entity->chunk->x - player.chunk->x) <= 1 && std::abs(entity->chunk->z - player.chunk->z) <= 1 && player.collides(entity))
                {
                    // Resolve the collision from the player's perspective
                    player.resolve_collision(entity);
                }
            }
        }
    }
#endif
    Vec3i block_pos = player.get_head_blockpos();
    Block *block = get_block_at(block_pos);
    if (block && properties(block->id).m_fluid && block_pos.y + 2 - get_fluid_height(this, block_pos, block->get_blockid()) >= player.aabb.min.y + player.y_offset)
    {
        player.in_fluid = properties(block->id).m_base_fluid;
    }
    else
    {
        player.in_fluid = BlockID::air;
    }

    if (should_destroy_block && player.mining_progress < 1.0f && player.raycast_target_found)
    {
        static Block *last_targeted_block = nullptr;
        Block *targeted_block = get_block_at(player.raycast_target_pos);
        if (last_targeted_block != targeted_block)
        {
            // Reset the progress if the targeted block has changed
            player.mining_progress = 0.0f;
            last_targeted_block = targeted_block;
        }

        if (targeted_block && targeted_block->get_blockid() != BlockID::air)
        {
            if (is_remote() && player.mining_tick == 0)
            {
                // Send block dig packet to the server
                for (uint8_t face_num = 0; face_num < 6; face_num++)
                {
                    if (player.raycast_target_face == face_offsets[face_num])
                    {
                        client->sendBlockDig(0, player.raycast_target_pos.x, player.raycast_target_pos.y, player.raycast_target_pos.z, (face_num + 4) % 6);
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
                    Sound sound = get_mine_sound(targeted_block->get_blockid());
                    sound.pitch *= 0.5f;
                    sound.volume = 0.15f;
                    sound.position = Vec3f(player.raycast_target_pos.x, player.raycast_target_pos.y, player.raycast_target_pos.z);
                    play_sound(sound);

                    // Add block breaking particles
                    Particle particle;
                    particle.max_life_time = 60;
                    particle.physics = PPHYSIC_FLAG_ALL;
                    particle.type = PTYPE_BLOCK_BREAK;
                    particle.size = 8;
                    particle.brightness = 0xFF;
                    int texture_index = get_default_texture_index(targeted_block->get_blockid());
                    int u = PARTICLE_X(texture_index);
                    int v = PARTICLE_Y(texture_index);

                    bool hitbox = false;
                    AABB &target_bounds = player.raycast_target_bounds;
                    javaport::Random rng;

                    for (int i = 0; hitbox && i < 8; i++)
                    {
                        // Randomize the particle position and velocity
                        Vec3f local_pos = Vec3f(rng.nextFloat(), rng.nextFloat(), rng.nextFloat());

                        Vec3f pos = (target_bounds.min + target_bounds.max) * 0.5 + local_pos;

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
                        pos.x = std::clamp(pos.x - 0.5, target_bounds.min.x, target_bounds.max.x) - 0.5;
                        pos.y = std::clamp(pos.y - 0.5, target_bounds.min.y, target_bounds.max.y);
                        pos.z = std::clamp(pos.z - 0.5, target_bounds.min.z, target_bounds.max.z) - 0.5;

                        particle.position = pos;

                        // Randomize the particle velocity
                        particle.velocity = Vec3f(rng.nextFloat() - .5f, rng.nextFloat() - .25f, rng.nextFloat() - .5f) * 3.0f;

                        // Randomize the particle texture coordinates
                        particle.u = u + (rng.next(2) << 2);
                        particle.v = v + (rng.next(2) << 2);

                        // Randomize the particle life time by up to 10 ticks
                        particle.life_time = particle.max_life_time - (rand() % 10);

                        add_particle(particle);
                    }
                }

                // Increase the mining progress
                player.mining_progress += properties(targeted_block->get_blockid()).get_break_multiplier(*player.selected_item, player.on_ground, basefluid(player.in_fluid) == BlockID::water);
            }
            else
            {
                player.mining_progress = 0.0f;
            }
        }
    }
    else
    {
        player.mining_progress = 0.0f;
        if (player.mining_tick < 0)
            player.mining_tick++;
        else
            player.mining_tick = 0;
    }
}

// Used to set the world to hell
void World::set_hell(bool hell)
{
    if (this->hell == hell)
        return;

    // Stop processing any light updates
    LightEngine::reset();
    if (hell)
    {
        // Set the light map to the nether light map
        memcpy(light_map, light_nether_rgba, 1024); // 256 * 4
        GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));
        GX_InvVtxCache();
    }

    // Disable sky light in hell
    LightEngine::enable_skylight(!hell);

    this->hell = hell;
}

Chunk *World::get_chunk_from_pos(const Vec3i &pos)
{
    return get_chunk(block_to_chunk_pos(pos));
}

Chunk *World::get_chunk(const Vec2i &pos)
{
    return get_chunk(pos.x, pos.y);
}

Chunk *World::get_chunk(int32_t x, int32_t z)
{
    uint64_t key = uint32_pair(x, z);
    if (auto it = chunk_cache.find(key); it != chunk_cache.end())
    {
        return it->second;
    }

    return nullptr;
}

void World::save_chunk(Chunk *chunk)
{
    if (!chunk)
        return;

    Lock chunk_lock(chunk_mutex);

    // Remove the chunk from the active list
    chunks.erase(std::remove(chunks.begin(), chunks.end(), chunk), chunks.end());

    // Remove the chunk from the cache
    uint64_t key = uint32_pair(chunk->x, chunk->z);
    chunk_cache.erase(key);

    chunk->state = is_remote() ? ChunkState::invalid : ChunkState::saving;

    // Send the chunk to the chunk manager
    pending_chunks.push_back(chunk);
}

bool World::add_chunk(int32_t x, int32_t z)
{
    if (!run_chunk_manager || pending_chunks.size() + chunks.size() >= CHUNK_COUNT)
        return false;
    Lock chunk_lock(chunk_mutex);

    // Function to find a chunk with the given coordinates
    auto find_chunk = [x, z](Chunk *chunk)
    {
        return chunk && chunk->x == x && chunk->z == z;
    };

    // Check if the chunk already exists
    if (std::find_if(pending_chunks.begin(), pending_chunks.end(), find_chunk) != pending_chunks.end())
        return false;
    if (std::find_if(chunks.begin(), chunks.end(), find_chunk) != chunks.end())
        return false;

    Chunk *chunk = new Chunk(x, z, this);

    // Check if the chunk exists in the region file
    mcr::Region *region = mcr::get_region(x >> 5, z >> 5);
    if (region->locations[(x & 31) | ((z & 31) << 5)] != 0)
        chunk->state = ChunkState::loading;

    // Send the chunk to the chunk manager
    pending_chunks.push_back(chunk);

    return true;
}

void World::deinit_chunk_manager()
{
    if (chunk_manager_thread_handle == LWP_THREAD_NULL)
        return;

    // Tell the chunk manager thread to stop
    run_chunk_manager = false;

    LWP_JoinThread(chunk_manager_thread_handle, NULL);
    chunk_manager_thread_handle = LWP_THREAD_NULL;

    // Cleanup the chunk mutex
    Lock::destroy(chunk_mutex);
}

void World::init_chunk_manager(ChunkProvider *chunk_provider)
{
    if (chunk_manager_thread_handle != LWP_THREAD_NULL)
        return;

    // Chunk provider can be null if something else will be providing the chunks.
    // This should only happen when in a remote world which means that chunks are
    // provided via the network code. In such a case, don't start the thread.
    if (!chunk_provider)
    {
        return;
    }
    auto chunk_manager_thread = [](void *arg) -> void *
    {
        World *world = (World *)arg;
        ChunkProvider *provider = world->chunk_provider;
        world->run_chunk_manager = true;
        while (world->run_chunk_manager || !world->pending_chunks.empty())
        {
            while (world->pending_chunks.empty())
            {
                LWP_YieldThread();
                if (!world->run_chunk_manager)
                {
                    return NULL;
                }
            }
            Chunk *chunk = world->pending_chunks.back();
            switch (chunk->state)
            {
            case ChunkState::loading:
            case ChunkState::empty:
            {
                // Generate the base terrain for the chunk
                provider->provide_chunk(chunk);
            }
            // Fall to the next case immediately: features
            case ChunkState::features:
            {
                // Move the chunk to the active list
                Lock chunk_lock(world->chunk_mutex);
                world->chunks.push_back(chunk);
                world->pending_chunks.erase(std::find(world->pending_chunks.begin(), world->pending_chunks.end(), chunk));
                uint64_t key = uint32_pair(chunk->x, chunk->z);
                world->chunk_cache.insert_or_assign(key, chunk);
                chunk_lock.unlock();

                // Finish the chunk with features
                provider->populate_chunk(chunk);
                break;
            }
            case ChunkState::saving:
            {
                try
                {
                    chunk->write();
                }
                catch (std::runtime_error &e)
                {
                    debug::print("Failed to save chunk: %s\n", e.what());
                }
            }
            // Fall to removal case after saving
            case ChunkState::invalid:
            {
                Lock chunk_lock(world->chunk_mutex);
                world->pending_chunks.erase(std::find(world->pending_chunks.begin(), world->pending_chunks.end(), chunk));
                delete chunk;
                break;
            }
            default:
            {
                // Empty the queue to avoid a rare deadlock
                if (!world->run_chunk_manager)
                {
                    Lock chunk_lock(world->chunk_mutex);
                    world->pending_chunks.erase(std::find(world->pending_chunks.begin(), world->pending_chunks.end(), chunk));
                    delete chunk;
                }
                break;
            }
            }

            usleep(100);
        }
        return NULL;
    };

    LWP_CreateThread(&chunk_manager_thread_handle,
                     chunk_manager_thread,
                     this,
                     NULL,
                     0,
                     50);
}

BlockID World::get_block_id_at(const Vec3i &position, BlockID default_id)
{
    Block *block = get_block_at(position);
    if (!block)
        return default_id;
    return block->get_blockid();
}

Block *World::get_block_at(const Vec3i &position)
{
    if (position.y < 0 || position.y > MAX_WORLD_Y)
        return nullptr;
    Chunk *chunk = get_chunk_from_pos(position);
    if (!chunk)
        return nullptr;
    return chunk->get_block(position);
}

uint8_t World::get_meta_at(const Vec3i &position)
{
    Block *block = get_block_at(position);
    if (!block)
        return 0;
    return block->meta;
}

void World::get_neighbors(const Vec3i &pos, Block **neighbors)
{
    for (int x = 0; x < 6; x++)
        neighbors[x] = get_block_at(pos + face_offsets[x]);
}

void World::set_block_at(const Vec3i &pos, BlockID id)
{
    Block *block = get_block_at(pos);
    if (block)
    {
        if (block->get_blockid() == id)
            return;
        block->set_blockid(id);
        block->meta = 0;
        BlockProperties &prop = properties(id);
        if (id != BlockID::air && prop.m_added)
            prop.m_added(this, pos, *block);
        mark_block_dirty(pos);
    }
}

void World::set_meta_at(const Vec3i &pos, uint8_t meta)
{
    Block *block = get_block_at(pos);
    if (block)
    {
        if (block->meta == meta)
            return;

        block->meta = meta;
        mark_block_dirty(pos);
    }
}

void World::set_block_and_meta_at(const Vec3i &pos, BlockID id, uint8_t meta)
{
    Block *block = get_block_at(pos);
    if (block)
    {
        if (block->get_blockid() == id && block->meta == meta)
            return;
        block->set_blockid(id);
        block->meta = meta;
        BlockProperties &prop = properties(id);
        if (id != BlockID::air && prop.m_added)
            prop.m_added(this, pos, *block);
        mark_block_dirty(pos);
    }
}

void World::replace_air_at(Vec3i pos, BlockID id)
{
    if (get_block_id_at(pos) != BlockID::air)
        return;
    set_block_at(pos, id);
}

void World::notify_at(const Vec3i &pos)
{
    if (!is_remote())
        notify_neighbors(pos);
}

void World::notify_neighbors(const Vec3i &pos)
{
    for (int i = 0; i < 6; i++)
    {
        Vec3i neighbour = pos + face_offsets[i];
        Block *block = get_block_at(neighbour);
        if (block)
        {
            BlockProperties &prop = properties(block->id);
            if (prop.m_neighbor_changed)
            {
                prop.m_neighbor_changed(this, neighbour, *block);
            }
        }
    }
}

void World::mark_block_dirty(const Vec3i &pos)
{
    if (pos.y > MAX_WORLD_Y || pos.y < 0)
        return;
    Chunk *chunk = get_chunk_from_pos(pos);
    if (!chunk)
        return;
    chunk->update_height_map(pos);
    LightEngine::post(pos);
}

TileEntity *World::get_tile_entity(const Vec3i &position)
{
    Chunk *chunk = get_chunk_from_pos(position);
    if (!chunk)
        return nullptr;
    return chunk->get_tile_entity(position);
}

void World::add_entity(EntityPhysical *entity)
{
    // If the world is local, assign a unique entity ID to prevent conflicts
    if (!is_remote())
    {
        while (world_entities.find(entity->entity_id) != world_entities.end())
        {
            entity->entity_id++;
        }
    }
    world_entities[entity->entity_id] = entity;
    Vec3i entity_pos = Vec3i(int(std::floor(entity->position.x)), int(std::floor(entity->position.y)), int(std::floor(entity->position.z)));
    entity->chunk = get_chunk_from_pos(entity_pos);
    if (entity->chunk)
        entity->chunk->entities.push_back(entity);
    entity->world = this;
}

void World::remove_entity(int32_t entity_id)
{
    EntityPhysical *entity = get_entity_by_id(entity_id);
    if (!entity)
    {
        debug::print("Removing unknown entity %d\n", entity_id);
        return;
    }
    entity->dead = true;
    if (!entity->chunk)
    {
        if (world_entities.find(entity_id) != world_entities.end())
            world_entities.erase(entity_id);
        if (dynamic_cast<EntityPlayerLocal *>(entity) == nullptr)
            delete entity;
    }
}

EntityPhysical *World::get_entity_by_id(int32_t entity_id)
{
    try
    {
        return world_entities.at(entity_id);
    }
    catch (std::out_of_range &)
    {
        return nullptr;
    }
}

void World::add_particle(const Particle &particle)
{
    m_particle_system.add_particle(particle);
}

void World::play_sound(const Sound &sound)
{
    if (!m_sound_system)
        return;
    m_sound_system->play_sound(sound);
}
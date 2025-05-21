#include <algorithm>
#include <unistd.h>
#include <ported/Random.hpp>
#include <crapper/client.hpp>

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

extern Crapper::MinecraftClient client;
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
}

world::~world()
{
    light_engine::deinit();
    delete player.m_entity;
    if (chunk_provider)
        delete chunk_provider;
}

bool world::is_remote()
{
    return remote;
}

void world::set_remote(bool value)
{
    remote = value;
}

void world::tick()
{
    update_entities();
}

void world::update()
{
    bool should_update_vbos = true;
    if (!is_remote() && !light_engine::busy())
    {
        should_update_vbos = prepare_chunks(1);
    }
    cleanup_chunks();
    edit_blocks();
    update_chunks();
    if (should_update_vbos)
        update_vbos();

    // Update the particle system
    m_particle_system.update(0.025);

    // Calculate chunk memory usage
    memory_usage = 0;
    for (chunk_t *&chunk : get_chunks())
    {
        memory_usage += chunk ? chunk->size() : 0;
    }

    m_sound_system.update(angles_to_vector(0, yrot + 90), player.m_entity->get_position(std::fmod(partial_ticks, 1)));
}

void world::update_chunks()
{
    int light_up_calls = 0;
    for (chunk_t *&chunk : get_chunks())
    {
        if (chunk)
        {
            float hdistance = chunk->player_taxicab_distance();
            if (hdistance > RENDER_DISTANCE * 24 + 24 && !is_remote())
            {
                remove_chunk(chunk);
                continue;
            }

            if (chunk->generation_stage != ChunkGenStage::done)
                continue;

            int min_height = player_pos.y - 16;
            for (int i = 0; i < 256; i++)
            {
                if (int(chunk->height_map[i]) < min_height)
                    min_height = chunk->height_map[i];
            }

            float horizontal_multiplier = std::abs(xrot) < 70 ? 16 : 12;
            float vertical_multiplier = std::abs(xrot) > 20 ? 12 : 8;
            bool visible = (hdistance <= std::max(RENDER_DISTANCE * horizontal_multiplier, 16.0f));

            // Tick chunks
            for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
            {
                chunkvbo_t &vbo = chunk->vbos[j];
                vbo.x = chunk->x * 16;
                vbo.y = j * 16;
                vbo.z = chunk->z * 16;
                float vdistance = std::abs(vbo.y - player_pos.y);

                vec3f vbo_offset = vec3f(vbo.x + 8, vbo.y + 8, vbo.z + 8) - player_pos;
                vec3f forward = angles_to_vector(xrot, yrot);
                bool behind = vbo_offset.x * forward.x + vbo_offset.y * forward.y + vbo_offset.z * forward.z < -16;

                bool above_ground = player_pos.y >= min_height;

                vbo.visible = visible && !behind && (above_ground || (vdistance <= std::max(RENDER_DISTANCE * vertical_multiplier, 16.0f))) && (vbo.y + 16 >= min_height);
                if (!is_remote() && chunk->has_fluid_updates[j] && vdistance <= SIMULATION_DISTANCE * 16 && ticks - last_fluid_tick >= 5)
                    update_fluids(chunk, j);
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
}

void world::update_fluids(chunk_t *chunk, int section)
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
        block_t *block = &chunk->blockstates[i | (section << 12)];
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
    chunk->has_fluid_updates[section] = (curr_fluid_count != 0);
}

void world::update_vbos()
{
    static std::vector<chunkvbo_t *> vbos_to_update;
    std::vector<chunkvbo_t *> vbos_to_rebuild;
    if (!light_engine::busy())
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
                    chunkvbo_t &vbo = chunk->vbos[j];
                    if (vbo.visible && vbo.dirty)
                    {
                        vbos_to_rebuild.push_back(&vbo);
                    }
                }
            }
        }
        std::sort(vbos_to_rebuild.begin(), vbos_to_rebuild.end(), [](chunkvbo_t *a, chunkvbo_t *b)
                  { return *a < *b; });
        uint32_t max_vbo_updates = 1;
        for (chunkvbo_t *vbo_ptr : vbos_to_rebuild)
        {
            chunkvbo_t &vbo = *vbo_ptr;
            int vbo_i = vbo.y >> 4;
            chunk_t *chunk = get_chunk_from_pos(vec3i(vbo.x, 0, vbo.z));
            vbo.dirty = false;
            chunk->recalculate_section_visibility(vbo_i);
            chunk->build_vbo(vbo_i, false);
            chunk->build_vbo(vbo_i, true);
            vbos_to_update.push_back(vbo_ptr);
            if (!--max_vbo_updates)
                break;
        }
    }
    // Update cached buffers when no vbos need to be rebuilt.
    // This ensures that the buffers are updated synchronously.
    if (vbos_to_rebuild.size() == 0)
    {
        for (chunkvbo_t *vbo_ptr : vbos_to_update)
        {
            chunkvbo_t &vbo = *vbo_ptr;
            if (vbo.solid != vbo.cached_solid)
            {
                // Clear the cached buffer
                vbo.cached_solid.clear();

                // Set the cached buffer to the new buffer
                vbo.cached_solid = vbo.solid;
            }
            if (vbo.transparent != vbo.cached_transparent)
            {
                // Clear the cached buffer
                vbo.cached_transparent.clear();

                // Set the cached buffer to the new buffer
                vbo.cached_transparent = vbo.transparent;
            }
            vbo.has_updated = true;
        }
        vbos_to_update.clear();
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

    player.draw_block_outline = raycast_precise(vec3f(player_pos.x + .5, player_pos.y + .5, player_pos.z + .5), vec3f(forward.x, forward.y, forward.z), 4, &player.raycast_pos, &player.raycast_face, player.block_bounds);
    if (player.draw_block_outline)
    {
        BlockID new_blockid = should_destroy_block ? BlockID::air : selected_block.get_blockid();
        if (should_destroy_block || should_place_block)
        {
            block_t *targeted_block = get_block_at(player.raycast_pos);
            vec3i editable_pos = should_destroy_block ? (player.raycast_pos) : (player.raycast_pos + player.raycast_face);
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
                        if (!should_destroy_block && should_place_block)
                        {
                            editable_block->meta = new_blockid == BlockID::air ? 0 : selected_block.meta;
                            editable_block->set_blockid(new_blockid);
                        }
                    }
                    if (should_destroy_block)
                    {
                        editable_block->meta = new_blockid == BlockID::air ? 0 : selected_block.meta;
                        editable_block->set_blockid(new_blockid);
                    }
                }

                if (should_destroy_block)
                {
                    if (!is_remote())
                        destroy_block(editable_pos, &old_block);
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

    // Clear the place/destroy block flags to prevent placing blocks immediately.
    should_place_block = false;
    should_destroy_block = false;
}

int world::prepare_chunks(int count)
{
    const int center_x = (int(std::floor(player.m_entity->position.x)) >> 4);
    const int center_z = (int(std::floor(player.m_entity->position.z)) >> 4);
    const int start_x = center_x - GENERATION_DISTANCE;
    const int start_z = center_z - GENERATION_DISTANCE;

    for (int x = start_x, rx = -GENERATION_DISTANCE; count && rx <= GENERATION_DISTANCE; x++, rx++)
    {
        for (int z = start_z, rz = -GENERATION_DISTANCE; count && rz <= GENERATION_DISTANCE; z++, rz++)
        {
            if (std::abs(rx) + std::abs(rz) > ((RENDER_DISTANCE * 3) >> 1))
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
        chunkvbo_t &vbo = chunk->vbos[j];
        vbo.visible = false;

        if (vbo.solid && vbo.solid != vbo.cached_solid)
        {
            vbo.solid.clear();
        }
        if (vbo.transparent && vbo.transparent != vbo.cached_transparent)
        {
            vbo.transparent.clear();
        }

        vbo.cached_solid.clear();
        vbo.cached_transparent.clear();
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
    int u = TEXTURE_X(texture_index);
    int v = TEXTURE_Y(texture_index);
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
        client.sendPlaceBlock(targeted.x, targeted.y, targeted.z, (face + 4) % 6, new_block->id, 1, new_block->meta);
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
    if (!is_remote())
        explode(pos, power * 0.75f, near);

    javaport::Random rng;

    sound sound = get_sound("old_explode");
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
}

void world::draw(camera_t &camera)
{
    // Enable backface culling for terrain
    GX_SetCullMode(GX_CULL_BACK);

    // Prepare the transformation matrix
    transform_view(gertex::get_view_matrix(), guVector{0, 0, 0});

    // Prepare opaque rendering parameters
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    gertex::set_blending(gertex::GXBlendMode::normal);
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

    // Draw the solid pass
    if (opaque)
    {
        for (chunk_t *&chunk : chunks)
        {
            if (chunk && chunk->generation_stage == ChunkGenStage::done && chunk->lit_state)
            {
                for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
                {
                    chunkvbo_t &vbo = chunk->vbos[j];
                    if (!vbo.visible)
                        continue;

                    guVector chunkPos = {(f32)chunk->x * 16, (f32)j * 16, (f32)chunk->z * 16};
                    if (vbo.cached_solid.buffer && vbo.cached_solid.length)
                    {
                        transform_view(gertex::get_view_matrix(), chunkPos);
                        GX_CallDispList(vbo.cached_solid.buffer, vbo.cached_solid.length);
                    }
                }
                chunk->render_entities(partial_ticks, false);
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
                    chunkvbo_t &vbo = chunk->vbos[j];
                    if (!vbo.visible)
                        continue;

                    guVector chunkPos = {(f32)chunk->x * 16, (f32)j * 16, (f32)chunk->z * 16};
                    if (vbo.cached_transparent.buffer && vbo.cached_transparent.length)
                    {
                        transform_view(gertex::get_view_matrix(), chunkPos);
                        GX_CallDispList(vbo.cached_transparent.buffer, vbo.cached_transparent.length);
                    }
                }
                chunk->render_entities(partial_ticks, true);
            }
        }
    }

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw block outlines
    if (opaque && player.draw_block_outline)
    {
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        vec3f outline_pos = player.raycast_pos - vec3f(0.5, 0.5, 0.5);

        vec3f towards_camera = vec3f(player_pos) - outline_pos;
        towards_camera.normalize();
        towards_camera = towards_camera * 0.002;

        vec3f b_min = vec3f(player.raycast_pos.x, player.raycast_pos.y, player.raycast_pos.z) + vec3f(1.0, 1.0, 1.0);
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

        transform_view(gertex::get_view_matrix(), floor_b_min + towards_camera - vec3f(0.5, 0.5, 0.5));

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
    block_t *view_block = get_block_at(vec3i(std::round(player_pos.x), std::round(player_pos.y), std::round(player_pos.z)));
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

        if (!properties(selected_block.id).m_fluid && (render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
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

    uint32_t tex_x = TEXTURE_X(texture_index);
    uint32_t tex_y = TEXTURE_Y(texture_index);

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
                GX_Vertex(vertex_property_t(min + vec3f(size.x * k, size.y * i, size.z * j), 0, 0, 0, 0, 0, 191));
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(vertex_property_t(min + vec3f(size.x * i, size.y * k, size.z * j), 0, 0, 0, 0, 0, 191));
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(vertex_property_t(min + vec3f(size.x * i, size.y * j, size.z * k), 0, 0, 0, 0, 0, 191));
            }
        }
    }
    GX_EndGroup();
}

void world::save()
{
    // Save the world to disk if in singleplayer
    if (!is_remote())
    {
        try
        {
            for (chunk_t *c : get_chunks())
                c->serialize();
        }
        catch (std::exception &e)
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
    catch (std::exception &e)
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

    light_engine::reset();
    gui_dirtscreen *dirtscreen = new gui_dirtscreen(gertex::GXView());
    dirtscreen->set_text("Loading level");
    gui::set_gui(dirtscreen);
    loaded = false;
    time_of_day = 0;
    ticks = 0;
    last_entity_tick = 0;
    last_fluid_tick = 0;
    hell = false;
    remote = false;
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

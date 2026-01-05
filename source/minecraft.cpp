#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <cmath>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <util/debuglog.hpp>
#include <util/config.hpp>
#include <util/timers.hpp>
#include <render/render_gui.hpp>
#include <gui/gui_survival.hpp>
#include <gui/gui_dirtscreen.hpp>
#include <gui/gui_titlescreen.hpp>
#include <world/world.hpp>
#include <crafting/recipe_manager.hpp>
#include <util/input/input.hpp>
#include <util/string_utils.hpp>
#include <registry/registry.hpp>
#include <util/busy_wait.hpp>
#include <gui/gui_busy_wait.hpp>

#include "light_day_mono_rgba.h"
#include "light_night_mono_rgba.h"
#include "light_day_rgba.h"
#include "light_night_rgba.h"
#include "block/blocks.hpp"

void *frameBuffer[2] = {NULL, NULL};
static int fb = 0;
static GXRModeObj *rmode = NULL;

static World *current_world = nullptr;
static SoundSystem *sound_system = nullptr;

int frameCounter = 0;

u32 raw_wiimote_down = 0;
int shoulder_btn_frame_counter = 0;
bool should_destroy_block = false;
bool should_place_block = false;

int cursor_x = 0;
int cursor_y = 0;

float fog_depth_multiplier = 1.0f;
float fog_light_multiplier = 1.0f;

void UpdateLoadingStatus(Progress *prog);
void HandleGUI(gertex::GXView &viewport);
void UpdateGUI(gertex::GXView &viewport);
void DrawGUI(gertex::GXView &viewport);
void DrawHUD(gertex::GXView &viewport);
void DrawDebugInfo(gertex::GXView &viewport);
void UpdateCamera();
void UpdateFog();
void GetInput();
void MainGameLoop();
//---------------------------------------------------------------------------------
bool isExiting = false;
s8 HWButton = -1;
void WiiResetPressed(u32, void *)
{
    HWButton = SYS_RETURNTOMENU;
}

void WiiPowerPressed()
{
    HWButton = SYS_POWEROFF;
}

void WiimotePowerPressed(s32 chan)
{
    HWButton = SYS_POWEROFF;
}

uint8_t light_map[1024] ATTRIBUTE_ALIGN(32) = {0};

void LightMapBlend(const uint8_t *src0, const uint8_t *src1, uint8_t *dst, uint8_t factor)
{
    uint8_t inv_factor = 255 - factor;
    for (int i = 0; i < 1024; i++)
    {
        *(dst++) = ((*(src0++) * int(inv_factor)) + ((*(src1++) * int(factor)))) / 255;
    }
}

void InitFailure(std::string message)
{
    if (frameBuffer[0])
        free(frameBuffer[0]);

    if (!rmode)
        rmode = VIDEO_GetPreferredMode(NULL);

    VIDEO_Configure(rmode);

    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    CON_Init(frameBuffer[0], 0, 0, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    VIDEO_ClearFrameBuffer(rmode, frameBuffer[0], COLOR_BLACK);
    printf("Initialization failed: %s\n\nPress the power or reset button to exit.\n", message.c_str());

    VIDEO_SetNextFramebuffer(frameBuffer[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();

    while (1)
    {
        if (HWButton != -1)
        {
            SYS_ResetSystem(HWButton, 0, 0);
            exit(0);
        }
        VIDEO_WaitVSync();
    }
}

void MainGameLoop()
{
    if (!current_world)
        return;

    set_render_world(current_world);

    Configuration config;
    try
    {
        // Attempt to load the Configuration file from the default location
        config.load();
    }
    catch (std::runtime_error &e)
    {
        debug::print("Using config defaults. %s\n", e.what());
    }

    // Initialize rendering settings from config
    f32 FOV = config.get<float>("fov", 90.0f);
    gertex::GXState state = gertex::get_state();
    Camera &camera = get_camera();
    camera.fov = FOV;
    camera.aspect = state.view.aspect / state.view.aspect_correction;
    camera.near = state.view.near;
    camera.far = state.view.far;
    bool mono_lighting = ((int)config.get("mono_lighting", 0) != 0);
    bool smooth_lighting = ((int)config.get("smooth_lighting", 0) != 0);
    bool vsync = ((int)config.get("vsync", 0) != 0);
    bool fast_leaves = ((int)config.get("fast_leaves", 0) != 0);
    render_fast_leaves = fast_leaves;
    current_world->sync_chunk_updates = ((int)config.get("sync_chunk_updates", 0) != 0);
    set_smooth_lighting(smooth_lighting);

    // Generate a "unique" username based on the device ID
    uint32_t dev_id = 0;
    ES_GetDeviceID(&dev_id);

    current_world->add_entity(&current_world->player);

    current_world->seed = gettime();

    // Use the username from the config or generate a default one
    current_world->player.player_name = std::string(config.get<std::string>("username", "Wii_" + std::to_string(dev_id)));

    current_world->set_sound_system(sound_system);

    // Setup the loading screen
    Progress prog;
    GuiDirtscreen *dirtscreen = new GuiDirtscreen;
    dirtscreen->set_text("Loading level\n\n\nBuilding terrain");
    dirtscreen->set_progress(&prog);
    Gui::set_gui(dirtscreen);

    // Fall back to local world if not connected to a server
    if (!current_world->is_remote())
    {
        if (!current_world->load())
        {
            current_world->create();
        }
    }

    bool in_game = true;

    // Begin the main loop
    while (!isExiting && in_game && HWButton == -1)
    {
        uint64_t frame_start = time_get();

        // Update the light map
        if (!current_world->hell)
        {
            LightMapBlend(mono_lighting ? light_day_mono_rgba : light_day_rgba, mono_lighting ? light_night_mono_rgba : light_night_rgba, light_map, 255 - uint8_t(get_sky_multiplier() * 255));
        }
        GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));
        GX_InvVtxCache();

        GetInput();

        for (uint32_t i = current_world->last_entity_tick, count = 0; i < current_world->ticks && count < 10; i++, count++)
        {
            update_textures();

            if (!current_world->tick())
            {
                // If the world tick fails (e.g. due to an error), exit the main loop
                in_game = false;
                break;
            }
        }

        UpdateCamera();

        current_world->update();

        UpdateLoadingStatus(&prog);

        state = gertex::get_state();
        gertex::perspective(state.view);
        // Draw the scene
        if (current_world->loaded)
        {
            UpdateFog();

            // Draw sky
            if (current_world->player.in_fluid == BlockID::air && !current_world->hell)
                draw_sky();

            GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
            current_world->draw(get_camera());

            use_texture(terrain_texture);
            current_world->draw_selected_block();
        }

        HandleGUI(state.view);
        GX_DrawDone();

        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
#ifdef DEBUG
        debug::copy_to(frameBuffer[fb]);
#endif
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        if (vsync)
            VIDEO_WaitVSync(); // Wait for vertical sync - if the frame lasts too long (which it often does) it will kill the frame rate
        else
            usleep(1000); // Give other tasks a chance to run - 1 ms seems sufficient

        // Swap buffers
        fb ^= 1;

        // Update time counters

        frameCounter++;
        current_world->delta_time = time_diff_s(frame_start, time_get());

        // Ensure that the delta time is not too large to prevent issues
        if (current_world->delta_time > 0.05)
            current_world->delta_time = 0.05;

        current_world->partial_ticks += current_world->delta_time * 20.0;
        current_world->time_of_day += int(current_world->partial_ticks);
        current_world->time_of_day %= 24000;
        current_world->ticks += int(current_world->partial_ticks);
        current_world->partial_ticks -= int(current_world->partial_ticks);
    }
    current_world->remove_entity(current_world->player.entity_id);

    dirtscreen = new GuiDirtscreen;
    dirtscreen->set_text("Saving level...");
    dirtscreen->set_progress(&prog);

    auto save_func = [&prog]()
    {
        current_world->save(&prog);
    };
    busy_wait(save_func, dirtscreen);
    delete current_world;
    current_world = nullptr;

    config.save();
}

int main(int argc, char **argv)
{
    VIDEO_Init();
    WPAD_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    SYS_SetPowerCallback(WiiPowerPressed);
    SYS_SetResetCallback(WiiResetPressed);
    WPAD_SetPowerButtonCallback(WiimotePowerPressed);

    if (!fatInitDefault())
        InitFailure("Failed to initialize FAT filesystem");

    // Allocate 2 framebuffers for double buffering
    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    // Configure video
    VIDEO_Configure(rmode);
    VIDEO_ClearFrameBuffer(rmode, frameBuffer[0], COLOR_BLACK);
    VIDEO_SetNextFramebuffer(frameBuffer[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    gertex::init(rmode);
    input::init();

    registry::register_all();

    init_face_normals();
    update_textures();

    gertex::GXState state = gertex::get_state();

    // Set the inventory cursor to the center of the screen
    cursor_x = state.view.width / 2;
    cursor_y = state.view.height * state.view.aspect_correction / 2;

    sound_system = new SoundSystem();

    Gui::init_matrices(state.view.aspect_correction);

    auto init_recipes = []()
    {
        crafting::RecipeManager::instance();
    };
    busy_wait(init_recipes, new GuiBusyWait("Building recipes"));

    while (!isExiting)
    {
        GuiTitleScreen *titlescreen = new GuiTitleScreen(&current_world);
        Gui::set_gui(titlescreen);
        set_render_world(nullptr);

        // Initialize the light map for block rendering
        std::memcpy(light_map, light_day_mono_rgba, 1024);
        DCFlushRange(light_map, 1024);
        GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));
        GX_InvVtxCache();

        // Enter the main menu loop
        while (!isExiting && Gui::get_gui() && !current_world)
        {
            Gui::get_gui()->sound_system = sound_system;
            GetInput();
            HandleGUI(state.view);
            GX_DrawDone();
            sound_system->update(sound_system->head_right, sound_system->head_position, false);

            GX_CopyDisp(frameBuffer[fb], GX_TRUE);
#ifdef DEBUG
            debug::copy_to(frameBuffer[fb]);
#endif
            VIDEO_SetNextFramebuffer(frameBuffer[fb]);
            VIDEO_Flush();
            VIDEO_WaitVSync();

            // Swap buffers
            fb ^= 1;
            if (HWButton != -1)
            {
                isExiting = true;
            }
            if (Gui::get_gui() == nullptr || Gui::get_gui()->quitting)
                break;
        }
        if (current_world)
        {
            MainGameLoop();
            isExiting = false;
            continue;
        }

        isExiting |= !Gui::get_gui();
    }
    Crapper::deinitNetwork();
    delete sound_system;
    input::deinit();
    WPAD_Shutdown();
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (HWButton != -1)
    {
        GX_Flush();
        SYS_ResetSystem(HWButton, 0, 0);
    }
    return 0;
}

void UpdateLoadingStatus(Progress *prog)
{
    if (!current_world)
        return;
#ifdef NO_LOADING_SCREEN
    current_world->loaded = true;
#endif
    bool is_loading = false;
    if (!current_world->loaded)
    {
        uint8_t loading_progress = 0;

        // Check if a 3x3 chunk area around the player is loaded

        Vec3i player_chunk_pos = current_world->player.get_foot_blockpos();

        int min_y = std::max(0, (player_chunk_pos.y >> 4) - 1);
        int max_y = std::min(VERTICAL_SECTION_COUNT - 1, (player_chunk_pos.y >> 4) + 1);
        int required = 0;
        int chunk_count = 0;

        for (int x = -1; x <= 1; x++)
        {
            for (int z = -1; z <= 1; z++)
            {
                Vec3i chunk_pos = player_chunk_pos + Vec3i(x * 16, 0, z * 16);
                Chunk *chunk = current_world->get_chunk_from_pos(chunk_pos);
                if (!chunk || chunk->state != ChunkState::done)
                {
                    continue;
                }

                chunk_count++;

                // Check if the vbos near the player are up to date
                for (int i = min_y; i <= max_y; i++)
                {
                    if (chunk->sections[i].visible)
                    {
                        required++;
                        if (chunk->sections[i].has_updated)
                        {
                            loading_progress++;
                        }
                    }
                }
            }
        }

        is_loading = loading_progress < required;
        if (chunk_count < 9)
        {
            is_loading = true;
            loading_progress = 0;
        }
        if (prog)
        {
            prog->progress = loading_progress;
            prog->max_progress = required;
        }
        if (!is_loading)
        {
            current_world->loaded = true;
            Gui::set_gui(nullptr);
        }
    }
}

void GetInput()
{
    WPAD_ScanPads();
    if ((WPAD_ButtonsDown(0) & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME)))
        isExiting = true;

    Vec3f right_stick = Vec3f(0, 0, 0);

    for (input::Device *dev : input::devices)
    {
        dev->scan();
        if (dev->connected())
            right_stick = right_stick + dev->get_right_stick();
    }
    should_place_block = false;
    should_destroy_block = false;

    if (!Gui::get_gui())
    {
        EntityPlayerLocal &player = current_world->player;
        player.rotation.y -= right_stick.x * current_world->delta_time * input::sensitivity;
        player.rotation.x += right_stick.y * current_world->delta_time * input::sensitivity;

        if (player.rotation.y > 360.f)
            player.rotation.y -= 360.f;
        if (player.rotation.y < 0)
            player.rotation.y += 360.f;
        if (player.rotation.x > 90.f)
            player.rotation.x = 90.f;
        if (player.rotation.x < -90.f)
            player.rotation.x = -90.f;

        bool left_shoulder_pressed = false;
        bool right_shoulder_pressed = false;
        bool hotbar_slot_changed = false;

        for (input::Device *dev : input::devices)
        {
            if (!dev->connected())
                continue;

            if ((dev->get_buttons_held() & input::BUTTON_PLACE) && ((dev->get_buttons_down() & input::BUTTON_PLACE) || shoulder_btn_frame_counter % 10 == 0))
                left_shoulder_pressed = true;
            if ((dev->get_buttons_held() & input::BUTTON_MINE))
                right_shoulder_pressed = true;
            if ((dev->get_buttons_down() & input::BUTTON_HOTBAR_LEFT))
            {
                current_world->player.selected_hotbar_slot = (current_world->player.selected_hotbar_slot + 8) % 9;
                hotbar_slot_changed = true;
            }
            if ((dev->get_buttons_down() & input::BUTTON_HOTBAR_RIGHT))
            {
                current_world->player.selected_hotbar_slot = (current_world->player.selected_hotbar_slot + 1) % 9;
                hotbar_slot_changed = true;
            }
        }
        if (current_world->is_remote() && hotbar_slot_changed)
            current_world->client->sendBlockItemSwitch(current_world->player.selected_hotbar_slot);
        if (left_shoulder_pressed || right_shoulder_pressed)
        {
            // Increment the shoulder button frame counter while a shoulder button is held
            shoulder_btn_frame_counter++;
        }
        else
        {
            // Reset the shoulder button frame counter if no shoulder button is pressed
            shoulder_btn_frame_counter = -1;
        }
        if (right_shoulder_pressed)
            should_destroy_block = true;
        else if (left_shoulder_pressed)
        {
            // repeats buttons every 10 frames
            if (shoulder_btn_frame_counter >= 0 && shoulder_btn_frame_counter % 10 == 0)
            {
                should_place_block = true;
            }
        }
    }
}

void HandleGUI(gertex::GXView &viewport)
{
    gertex::ortho(viewport);
    // Use 0 fractional bits for the position data, because we're drawing in pixel space.
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);

    // Disable fog
    gertex::use_fog(false);

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
    gertex::set_blending(gertex::GXBlendMode::normal);
    GX_SetAlphaUpdate(GX_TRUE);

    for (input::Device *dev : input::devices)
    {
        if (!dev->connected())
            continue;

        if ((dev->get_buttons_down() & input::BUTTON_INVENTORY) != 0)
        {
            if (current_world && current_world->loaded)
            {
                if (Gui::get_gui())
                {
                    Gui::set_gui(nullptr);
                }
                else
                {
                    Gui::set_gui(new GuiSurvival(&current_world->player, nullptr));
                }
            }
        }
    }
    if (current_world && current_world->loaded)
    {
        // Draw the underwater overlay
        static Vec3f pan_underwater_texture(0, 0, 0);
        for (input::Device *dev : input::devices)
        {
            if (!dev->connected())
                continue;
            pan_underwater_texture.x += dev->get_right_stick().x * 0.25f;
            pan_underwater_texture.y += dev->get_right_stick().y * 0.25f;
        }
        float corrected_height = viewport.height * viewport.aspect_correction;
        pan_underwater_texture.x = std::fmod(pan_underwater_texture.x, viewport.width);
        pan_underwater_texture.y = std::fmod(pan_underwater_texture.y, corrected_height);
        static float vignette_strength = 0;
        vignette_strength = lerpf(vignette_strength, 1.0f - (current_world->player.light_level) / 15.0f, 0.01f);
        vignette_strength = std::clamp(vignette_strength, 0.0f, 1.0f);
        uint8_t vignette_alpha = 0xFF * vignette_strength;
        if (current_world->player.in_fluid == BlockID::water)
        {
            draw_textured_quad(underwater_texture, pan_underwater_texture.x - viewport.width, pan_underwater_texture.y - viewport.height, viewport.width * 3, corrected_height * 3, 0, 0, 48, 48);
        }
        use_texture(vignette_texture);
        draw_colored_sprite(vignette_texture, Vec2i(0, 0), Vec2i(viewport.width, corrected_height), 0, 0, 256, 256, GXColor{0xFF, 0xFF, 0xFF, vignette_alpha});

        DrawHUD(viewport);
    }
    UpdateGUI(viewport);
    DrawGUI(viewport);
    if (current_world)
        DrawDebugInfo(viewport);
}

GXColor fps_color(int fps)
{
    GXColor result = {0xFF, 0xFF, 0xFF, 0xFF};
    switch (fps / 10)
    {
    case 0:                                // 0-9 FPS
    case 1:                                // 10-19 FPS
        result = {0xFF, 0x55, 0x55, 0xFF}; // Red
        break;
    case 2:                                // 20-29 FPS
    case 3:                                // 30-39 FPS
        result = {0xFF, 0xAA, 0x00, 0xFF}; // Gold
        break;
    case 4:                                // 40-49 FPS
        result = {0xFF, 0xFF, 0x55, 0xFF}; // Yellow
        break;
    default:                               // 50+ FPS
        result = {0xFF, 0xFF, 0xFF, 0xFF}; // White
        break;
    }
    return result;
}

void DrawDebugInfo(gertex::GXView &viewport)
{
    uint64_t current_frame_time = time_get();
    static uint64_t last_frame_time = current_frame_time;
    static uint64_t last_sample_time = current_frame_time;
    static double fps = 0;

    // Update FPS every 1/4th second (get first sample ASAP i.e. on the second frame)
    if (time_diff_s(last_sample_time, current_frame_time) >= 0.25 || frameCounter == 1)
    {
        // Calculate the time difference in seconds
        fps = time_diff_s(last_frame_time, current_frame_time);

        // Avoid division by zero
        if (fps > 0)
        {
            // Calculate FPS
            fps = 1 / fps;
        }

        // Reset the last sample time
        last_sample_time = current_frame_time;
    }
    // Update the last frame time
    last_frame_time = current_frame_time;

    std::string memory_usage_str = "Chunk Memory Usage: ";

    // Get the current memory usage in bytes
    size_t memory_usage = current_world ? current_world->memory_usage : 0;
    if (memory_usage > 1024 * 1024)
    {
        // Convert to megabytes
        memory_usage_str += str::ftos(memory_usage / (1024.0 * 1024.0), 1) + " MB";
    }
    else
    {
        // Convert to kilobytes
        memory_usage_str += str::ftos(memory_usage / 1024.0, 1) + " KB";
    }

    // Display debug information
    Gui::draw_text_with_shadow(0, viewport.ystart, memory_usage_str);
    Gui::draw_text_with_shadow(0, viewport.ystart + 16, std::to_string(int(fps)) + " fps", fps_color(int(fps)));
    std::string resolution_str = std::to_string(int(viewport.width)) + "x" + std::to_string(int(viewport.height));
    std::string widescreen_str = viewport.widescreen ? " Widescreen" : "";
    Gui::draw_text_with_shadow(0, viewport.ystart + 32, resolution_str + widescreen_str);
}

void UpdateCamera()
{
    Camera &camera = get_camera();
    EntityPlayerLocal &player = current_world->player;

    // View bobbing
    static float view_bob_angle = 0;
    Vec3f target_view_bob_offset;
    Vec3f target_view_bob_screen_offset;
    vfloat_t view_bob_amount = 0.15;

    Vec3f h_velocity = Vec3f(player.velocity.x, 0, player.velocity.z);
    view_bob_angle += h_velocity.magnitude() * current_world->delta_time * 60;
    if (h_velocity.sqr_magnitude() > 0.001 && player.on_ground)
    {
        target_view_bob_offset = (Vec3f(0, std::abs(std::sin(view_bob_angle)) * view_bob_amount * 2, 0) + angles_to_vector(0, player.rotation.y + 90) * std::cos(view_bob_angle) * view_bob_amount);
        target_view_bob_screen_offset = view_bob_amount * Vec3f(std::sin(view_bob_angle), -std::abs(std::cos(view_bob_angle)), 0);
    }
    else
    {
        target_view_bob_offset = Vec3f(0);
        target_view_bob_screen_offset = Vec3f(0);
    }
    current_world->player.view_bob_offset = Vec3f::lerp(current_world->player.view_bob_offset, target_view_bob_offset, 0.035);
    current_world->player.view_bob_screen_offset = Vec3f::lerp(current_world->player.view_bob_screen_offset, target_view_bob_screen_offset, 0.035);
    camera.position = current_world->player.view_bob_offset + player.get_position(std::fmod(current_world->partial_ticks, 1));
    camera.rot.x = player.rotation.x;
    camera.rot.y = player.rotation.y;
    camera.rot.z = lerpf(camera.rot.z, 0, current_world->delta_time * 5);
    if (camera.rot.z < 0.1)
    {
        camera.rot.z = 0;
    }

    current_world->update_frustum(camera);
}

void UpdateFog()
{
    Camera &camera = get_camera();
    GXColor background = GXColor{0, 0, 0, 0xFF};
    gertex::GXState state = gertex::get_state();
    gertex::GXFog fog = gertex::GXFog{true, gertex::GXFogType::linear, state.view.near, state.view.far, state.view.near, state.view.far, background};

    background = get_fog_color();

    Block *block = current_world->get_block_at(current_world->player.get_head_blockpos());
    if (block)
        fog_light_multiplier = lerpf(fog_light_multiplier, std::pow(0.9f, (15.0f - block->sky_light)), 0.05f);

    fog_depth_multiplier = lerpf(fog_depth_multiplier, std::min(std::max(camera.position.y, 24.) / 36., 1.0), 0.05f);

    float fog_multiplier = current_world->hell ? 0.5f : 1.0f;
    if (current_world->player.in_fluid == BlockID::lava)
    {
        background = GXColor{0xFF, 0, 0, 0xFF};
        fog_multiplier = 0.05f;
    }
    else if (current_world->player.in_fluid == BlockID::water)
    {
        background = GXColor{0, 0, 0xFF, 0xFF};
        fog_multiplier = 0.6f;
    }

    // Apply the fog light multiplier
    background = background * fog_light_multiplier;

    // Set the background color
    GX_SetCopyClear(background, 0x00FFFFFF);
    fog.color = background;

    // Set fog near and far distances
    fog.start = fog_multiplier * fog_depth_multiplier * FOG_DISTANCE * 0.5f;
    fog.end = fog.start * 2.0f;

    // Enable fog
    gertex::set_fog(fog);
}

void UpdateGUI(gertex::GXView &viewport)
{
    if (!Gui::get_gui())
        return;
    if (Gui::get_gui()->use_cursor())
        for (input::Device *dev : input::devices)
        {
            if (dev->connected())
            {
                // Update the cursor position based on the left stick
                Vec3f left_stick = dev->get_left_stick();
                cursor_x += left_stick.x * 8;
                cursor_y -= left_stick.y * 8;

                // Since you can technically go out of bounds using a joystick, we need to clamp the cursor position; it'd be really annoying losing the cursor
                cursor_x = std::clamp(cursor_x, 0, int(viewport.width / viewport.aspect_correction));
                cursor_y = std::clamp(cursor_y, 0, int(viewport.height));
            }
        }
    Gui::get_gui()->update();
}

void DrawHUD(gertex::GXView &viewport)
{
    // Disable depth testing for GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);

    // Reset the orthogonal position matrix and push it onto the stack
    gertex::ortho(viewport);

    float corrected_height = viewport.height * viewport.aspect_correction;

    // Draw crosshair
    int crosshair_x = int(viewport.width - 32) >> 1;
    int crosshair_y = int(corrected_height - 32) >> 1;

    // Save the current state
    gertex::GXState state = gertex::get_state();

    // Use inverse blending for the crosshair
    gertex::set_blending(gertex::GXBlendMode::inverse);

    // Only draw when fully opaque
    gertex::set_alpha_cutoff(255);

    // Draw the crosshair
    draw_textured_quad(icons_texture, crosshair_x, crosshair_y, 32, 32, 0, 0, 16, 16);

    // Restore the state
    gertex::set_state(state);

    float pointer_x = 0.5f;
    float pointer_y = 0.5f;

    bool pointer_visible = false;

    // Draw IR cursor if visible
    for (input::Device *dev : input::devices)
    {
        if (dev->is_pointer_visible())
        {
            pointer_visible = true;
            pointer_x = dev->get_pointer_x();
            pointer_y = dev->get_pointer_y();
            break;
        }
    }
    if (pointer_visible)
    {
        draw_textured_quad(icons_texture, pointer_x * viewport.width - 7, pointer_y * corrected_height - 3, 48, 48, 32, 32, 56, 56);
    }

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw the hotbar background
    draw_textured_quad(gui_texture, (viewport.width - 364) / 2, corrected_height - 44, 364, 44, 0, 0, 182, 22);

    // Draw the hotbar selection
    draw_textured_quad(gui_texture, (viewport.width - 364) / 2 + current_world->player.selected_hotbar_slot * 40 - 2, corrected_height - 46, 48, 48, 0, 22, 24, 46);
    // Push the orthogonal position matrix onto the stack
    gertex::push_matrix();

    // Draw the hotbar items
    for (size_t i = 0; i < 9; i++)
    {
        Gui::draw_item((viewport.width - 364) / 2 + i * 40 + 6, corrected_height - 38, current_world->player.items[i + 36], viewport);
    }

    // Restore the orthogonal position matrix
    gertex::pop_matrix();
    gertex::load_pos_matrix();

    // Enable direct colors as the previous call to draw_item may have changed the color mode
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw the player's health above the hotbar. The texture size is 9x9 but they overlap by 1 pixel.
    int health = current_world->player.health;

    int empty_heart_offset = 16 + ((current_world->player.health_update_tick / 3) % 2) * 9;

    // Empty hearts
    for (int i = 0; i < 10; i++)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + i * 16, corrected_height - 64, 18, 18, empty_heart_offset, 0, empty_heart_offset + 9, 9);
    }

    // Full hearts
    for (int i = 0; i < (health & ~1); i += 2)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + i * 8, corrected_height - 64, 18, 18, 52, 0, 61, 9);
    }

    // Half heart (if the player has an odd amount of health)
    if (health > 0 && (health & 1) == 1)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + (health & ~1) * 8, corrected_height - 64, 18, 18, 61, 0, 70, 9);
    }
}

void DrawGUI(gertex::GXView &viewport)
{
    Gui *m_gui = Gui::get_gui();
    if (!m_gui)
        return;

    // Disable depth testing for GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);

    // Reset the orthogonal position matrix and push it onto the stack
    gertex::ortho(viewport);
    gertex::push_matrix();

    // Draw the GUI elements
    Gui::get_gui()->draw();

    // Restore the orthogonal position matrix
    gertex::pop_matrix();
    gertex::load_pos_matrix();

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    if (!Gui::get_gui()->use_cursor())
        return;

    bool pointer_visible = false;

    for (input::Device *dev : input::devices)
    {
        if (dev->is_pointer_visible())
        {
            pointer_visible = true;
            cursor_x = dev->get_pointer_x();
            cursor_y = dev->get_pointer_y();
        }
    }

    // Draw the cursor
    if (pointer_visible && !Gui::get_gui()->contains(cursor_x, cursor_y))
        draw_textured_quad(icons_texture, cursor_x - 7, cursor_y - 3, 32, 48, 32, 32, 48, 56);
    else
        draw_textured_quad(icons_texture, cursor_x - 16, cursor_y - 16, 32, 32, 0, 32, 32, 64);
}
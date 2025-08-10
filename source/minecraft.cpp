#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <cmath>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <ogc/conf.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "util/debuglog.hpp"
#include "util/config.hpp"
#include "block_id.hpp"
#include "timers.hpp"
#include "raycast.hpp"
#include "render_gui.hpp"
#include "inventory.hpp"
#include "gui_survival.hpp"
#include "gui_dirtscreen.hpp"
#include "world.hpp"
#include "util/input/keyboard_mouse.hpp"
#include "util/input/wiimote_nunchuk.hpp"
#include "util/input/wiimote_classic.hpp"
#include "util/string_utils.hpp"
#ifdef MONO_LIGHTING
#include "light_day_mono_rgba.h"
#include "light_night_mono_rgba.h"
#else
#include "light_day_rgba.h"
#include "light_night_rgba.h"
#endif

#define DEFAULT_FIFO_SIZE (256 * 1024)

void *frameBuffer[2] = {NULL, NULL};
static GXRModeObj *rmode = NULL;

f32 xrot = 0.0f;
f32 yrot = 0.0f;
guVector player_pos = {0.F, 80.0F, 0.F};

world *current_world = nullptr;

configuration config;

int frameCounter = 0;

u32 raw_wiimote_down = 0;
int shoulder_btn_frame_counter = 0;
bool should_destroy_block = false;
bool should_place_block = false;

int cursor_x = 0;
int cursor_y = 0;

float fog_depth_multiplier = 1.0f;
float fog_light_multiplier = 1.0f;

// Function to create the full directory path
int mkpath(const char *path, mode_t mode)
{
    char tmp[256]; // Temporary buffer to construct directories
    char *p = NULL;
    size_t len;

    // Copy the path to a temporary buffer
    strncpy(tmp, path, sizeof(tmp) - 1);
    len = strlen(tmp);

    // Remove the trailing slash if it exists
    if (tmp[len - 1] == '/')
    {
        tmp[len - 1] = '\0';
    }

    // Create directories step by step
    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0'; // Temporarily end the string to isolate the directory

            // Try to create the directory, ignore error if it already exists
            if (mkdir(tmp, mode) != 0 && errno != EEXIST)
            {
                printf("Error creating directory %s: %s\n", tmp, strerror(errno));
                return -1;
            }

            *p = '/'; // Restore the slash
        }
    }

    // Finally, create the full path
    if (mkdir(tmp, mode) != 0 && errno != EEXIST)
    {
        printf("Error creating directory %s: %s\n", tmp, strerror(errno));
        return -1;
    }

    return 0;
}
void UpdateLoadingStatus();
void HandleGUI(gertex::GXView &viewport);
void UpdateGUI(gertex::GXView &viewport);
void DrawGUI(gertex::GXView &viewport);
void DrawHUD(gertex::GXView &viewport);
void DrawDebugInfo(gertex::GXView &viewport);
void UpdateCamera(camera_t &camera);
void PrepareTEV();
void GetInput();
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

void init_fail(std::string message)
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

int main(int argc, char **argv)
{
    u32 fb = 0;
    f32 yscale;
    u32 xfbHeight;
    void *gpfifo = NULL;
    GXColor background = GXColor{0, 0, 0, 0xFF};

    VIDEO_Init();
    WPAD_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    SYS_SetPowerCallback(WiiPowerPressed);
    SYS_SetResetCallback(WiiResetPressed);
    WPAD_SetPowerButtonCallback(WiimotePowerPressed);

    if (!fatInitDefault())
        init_fail("Failed to initialize FAT filesystem");
    try
    {
        // Attempt to load the configuration file from the default location
        config.load();
    }
    catch (std::runtime_error &e)
    {
        printf("Using config defaults. %s\n", e.what());
    }
    // Allocate the fifo buffer
    gpfifo = memalign(32, DEFAULT_FIFO_SIZE);
    memset(gpfifo, 0, DEFAULT_FIFO_SIZE);

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
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
    fb ^= 1;
#ifdef DEBUG
    debug::init(rmode);
#endif
    // Init the GPU
    GX_Init(gpfifo, DEFAULT_FIFO_SIZE);

    // Clears the bg to color and clears the z buffer
    GX_SetCopyClear(background, 0x00FFFFFF);

    // other gx setup
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    xfbHeight = GX_SetDispCopyYScale(yscale);
#ifdef DEBUG
    GX_SetScissor(0, 0, rmode->fbWidth - 224, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth - 224, rmode->efbHeight);
#else
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
#endif
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
    if (rmode->aa)
    {
        GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
    }
    else
    {
        GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
    }
    GX_CopyDisp(frameBuffer[fb], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    GX_SetColorUpdate(GX_TRUE);

    // Setup the default vertex attribute table
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_CLR1, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR1, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    // Prepare TEV
    GX_SetNumChans(2);
    GX_SetChanCtrl(GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetChanAmbColor(GX_COLOR0, GXColor{0, 0, 0, 255});
    GX_SetChanMatColor(GX_COLOR0, GXColor{255, 255, 255, 255});
    GX_SetNumTexGens(1);

    GX_InvVtxCache();
    GX_InvalidateTexAll();

    init_textures();
    update_textures();
    // Init viewport params
    gertex::GXView viewport = gertex::GXView(rmode->fbWidth, rmode->efbHeight, CONF_GetAspectRatio(), 90, gertex::CAMERA_NEAR, gertex::CAMERA_FAR, yscale);

    // Set the inventory cursor to the center of the screen
    cursor_x = viewport.width / 2;
    cursor_y = viewport.height * viewport.aspect_correction / 2;

    bool vsync = ((int)config.get("vsync", 0) != 0);

    f32 FOV = config.get<float>("fov", 90.0f);

    camera_t camera = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}, // Camera position
        FOV,                // Field of view
        viewport.aspect,    // Aspect ratio
        viewport.near,      // Near clipping plane
        viewport.far        // Far clipping plane
    };

    input::init();
    input::add_device(new input::keyboard_mouse);
    input::add_device(new input::wiimote_nunchuk);
    input::add_device(new input::wiimote_classic);

    current_world = new world;

    // Generate a "unique" username based on the device ID
    uint32_t dev_id = 0;
    ES_GetDeviceID(&dev_id);

    entity_player_local *player_entity = current_world->player.m_entity;

    // Use the username from the config or generate a default one
    player_entity->player_name = std::string(config.get<std::string>("username", "Wii_" + std::to_string(dev_id)));

    // Add the player to the world - it should persist until the game is closed
    add_entity(player_entity);

    current_world->reset();
    current_world->seed = gettime();
    VIDEO_Flush();
    VIDEO_WaitVSync();
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetZCompLoc(GX_FALSE);
    GX_SetLineWidth(16, GX_VTXFMT0);
    player_pos.x = 0;
    player_pos.y = -1000;
    player_pos.z = 0;
    init_face_normals();
    PrepareTEV();

    inventory::init_items();
    gui::init_matrices(viewport.aspect_correction);

    gertex::GXFog fog = gertex::GXFog{true, gertex::GXFogType::linear, viewport.near, viewport.far, viewport.near, viewport.far, background};

    // Setup the loading screen
    gui_dirtscreen *dirtscreen = new gui_dirtscreen(viewport);
    dirtscreen->set_text("Loading level\n\n\nBuilding terrain");
    gui::set_gui(dirtscreen);

    // Optionally join a server if the configuration specifies one
    std::string server_ip = config.get<std::string>("server", "");

    if (!server_ip.empty() && Crapper::initNetwork())
    {
        int32_t server_port = config.get<int32_t>("port", 25565);

        try
        {
            // Attempt to connect to the server
            current_world->set_remote(true);
            current_world->client->joinServer(server_ip, server_port);
        }
        catch (std::runtime_error &e)
        {
            // If connection fails, set the world to local
            current_world->set_remote(false);
            printf("Failed to connect to server: %s\n", e.what());
        }
    }

    // Fall back to local world if not connected to a server
    if (!current_world->is_remote())
    {
        if (!current_world->load())
        {
            current_world->create();
        }
    }

    GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));

    while (!isExiting)
    {
        u64 frame_start = time_get();
        float sky_multiplier = get_sky_multiplier();
        if (HWButton != -1)
        {
            gui_dirtscreen *dirtscreen = new gui_dirtscreen(viewport);
            dirtscreen->set_text("Saving level...");
            gui::set_gui(dirtscreen);
            isExiting = true;
        }
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
        if (!current_world->hell)
        {
#ifdef MONO_LIGHTING
            LightMapBlend(light_day_mono_rgba, light_night_mono_rgba, light_map, 255 - uint8_t(sky_multiplier * 255));
#else
            LightMapBlend(light_day_rgba, light_night_rgba, light_map, 255 - uint8_t(sky_multiplier * 255));
#endif
            GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));
            GX_InvVtxCache();
        }
        background = get_sky_color();

        block_t *block = get_block_at(current_world->player.m_entity->get_head_blockpos());
        if (block)
            fog_light_multiplier = lerpf(fog_light_multiplier, std::pow(0.9f, (15.0f - block->sky_light)), 0.05f);

        fog_depth_multiplier = lerpf(fog_depth_multiplier, std::min(std::max(player_pos.y, 24.f) / 36.f, 1.0f), 0.05f);

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

        GetInput();

        for (uint32_t i = current_world->last_entity_tick, count = 0; i < current_world->ticks && count < 10; i++, count++)
        {
            update_textures();

            current_world->tick();
        }

        UpdateCamera(camera);

        current_world->update();

        UpdateLoadingStatus();

        gertex::perspective(viewport);

        // Set fog near and far distances
        fog.start = fog_multiplier * fog_depth_multiplier * FOG_DISTANCE * 0.5f;
        fog.end = fog.start * 2.0f;

        // Enable fog
        gertex::set_fog(fog);

        // Draw the scene
        if (current_world->loaded)
        {
            // Draw sky
            if (current_world->player.in_fluid == BlockID::air && !current_world->hell)
                draw_sky(background);

            GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
            current_world->draw(camera);

            use_texture(terrain_texture);
            current_world->draw_selected_block();
        }

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

        HandleGUI(viewport);
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

        fb ^= 1;
    }
    current_world->save();
    current_world->reset();
    Crapper::deinitNetwork();
    input::deinit();
    WPAD_Shutdown();
    delete current_world;
    config.save();
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (HWButton != -1)
    {
        GX_Flush();
        SYS_ResetSystem(HWButton, 0, 0);
    }
    return 0;
}

void UpdateLoadingStatus()
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

        vec3i player_chunk_pos = current_world->player.m_entity->get_foot_blockpos();

        int min_y = std::max(0, (player_chunk_pos.y >> 4) - 1);
        int max_y = std::min(VERTICAL_SECTION_COUNT - 1, (player_chunk_pos.y >> 4) + 1);
        int required = 0;
        int chunk_count = 0;

        for (int x = -1; x <= 1; x++)
        {
            for (int z = -1; z <= 1; z++)
            {
                vec3i chunk_pos = player_chunk_pos + vec3i(x * 16, 0, z * 16);
                chunk_t *chunk = get_chunk_from_pos(chunk_pos);
                if (!chunk || chunk->generation_stage != ChunkGenStage::done)
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

        gui_dirtscreen *dirtscreen = dynamic_cast<gui_dirtscreen *>(gui::get_gui());
        if (dirtscreen)
        {
            if (is_loading)
            {
                // Update the loading screen
                dirtscreen->set_progress(loading_progress, required);
            }
            else
            {
                // The world is loaded - remove the loading screen
                current_world->loaded = true;
                gui::set_gui(nullptr);
            }
        }
    }
}

void GetInput()
{
    WPAD_ScanPads();
    if ((WPAD_ButtonsDown(0) & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME)))
        isExiting = true;

    vec3f left_stick = vec3f(0, 0, 0);
    vec3f right_stick = vec3f(0, 0, 0);

    for (input::device *dev : input::devices)
    {
        dev->scan();
        if (dev->connected())
        {
            left_stick = left_stick + dev->get_left_stick();
            right_stick = right_stick + dev->get_right_stick();
        }
    }

    should_place_block = false;
    should_destroy_block = false;

    if (!gui::get_gui())
    {

        yrot -= right_stick.x * current_world->delta_time * input::sensitivity;
        xrot += right_stick.y * current_world->delta_time * input::sensitivity;

        if (yrot > 360.f)
            yrot -= 360.f;
        if (yrot < 0)
            yrot += 360.f;
        if (xrot > 90.f)
            xrot = 90.f;
        if (xrot < -90.f)
            xrot = -90.f;

        bool left_shoulder_pressed = false;
        bool right_shoulder_pressed = false;
        bool hotbar_slot_changed = false;

        for (input::device *dev : input::devices)
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
    for (input::device *dev : input::devices)
    {
        if (!dev->connected())
            continue;

        if ((dev->get_buttons_down() & input::BUTTON_INVENTORY) != 0)
        {
            if (current_world->loaded)
            {
                if (gui::get_gui())
                {
                    gui::set_gui(nullptr);
                }
                else
                {
                    gui::set_gui(new gui_survival(viewport, current_world->player.m_inventory));
                }
            }
        }
    }
    if (current_world->loaded)
    {
        // Draw the underwater overlay
        static vec3f pan_underwater_texture(0, 0, 0);
        for (input::device *dev : input::devices)
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
        vignette_strength = lerpf(vignette_strength, 1.0f - (current_world->player.m_entity->light_level) / 15.0f, 0.01f);
        vignette_strength = std::clamp(vignette_strength, 0.0f, 1.0f);
        uint8_t vignette_alpha = 0xFF * vignette_strength;
        if (current_world->player.in_fluid == BlockID::water)
        {
            draw_textured_quad(underwater_texture, pan_underwater_texture.x - viewport.width, pan_underwater_texture.y - viewport.height, viewport.width * 3, corrected_height * 3, 0, 0, 48, 48);
        }
        use_texture(vignette_texture);
        draw_colored_sprite(vignette_texture, vec2i(0, 0), vec2i(viewport.width, corrected_height), 0, 0, 256, 256, GXColor{0xFF, 0xFF, 0xFF, vignette_alpha});

        DrawHUD(viewport);
    }
    UpdateGUI(viewport);
    DrawGUI(viewport);

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
    size_t memory_usage = current_world->memory_usage;
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
    gui::draw_text_with_shadow(0, viewport.ystart, memory_usage_str);
    gui::draw_text_with_shadow(0, viewport.ystart + 16, std::to_string(int(fps)) + " fps", fps_color(int(fps)));
    std::string resolution_str = std::to_string(int(viewport.width)) + "x" + std::to_string(int(viewport.height));
    std::string widescreen_str = viewport.widescreen ? " Widescreen" : "";
    gui::draw_text_with_shadow(0, viewport.ystart + 32, resolution_str + widescreen_str);
}

void Render(guVector chunkPos, void *buffer, u32 length)
{
    if (!buffer || !length)
        return;
    transform_view(gertex::get_view_matrix(), chunkPos);
    // Render
    GX_CallDispList(buffer, length); // Draw the box
}

void UpdateCamera(camera_t &camera)
{
    entity_player_local *player = current_world->player.m_entity;
    player_pos = player->get_position(std::fmod(current_world->partial_ticks, 1));

    // View bobbing
    static float view_bob_angle = 0;
    vec3f target_view_bob_offset;
    vec3f target_view_bob_screen_offset;
    vfloat_t view_bob_amount = 0.15;

    vec3f h_velocity = vec3f(player->velocity.x, 0, player->velocity.z);
    view_bob_angle += h_velocity.magnitude();
    if (h_velocity.sqr_magnitude() > 0.001 && player->on_ground)
    {
        target_view_bob_offset = (vec3f(0, std::abs(std::sin(view_bob_angle)) * view_bob_amount * 2, 0) + angles_to_vector(0, yrot + 90) * std::cos(view_bob_angle) * view_bob_amount);
        target_view_bob_screen_offset = view_bob_amount * vec3f(std::sin(view_bob_angle), -std::abs(std::cos(view_bob_angle)), 0);
    }
    else
    {
        target_view_bob_offset = vec3f(0);
        target_view_bob_screen_offset = vec3f(0);
    }
    current_world->player.view_bob_offset = vec3f::lerp(current_world->player.view_bob_offset, target_view_bob_offset, 0.035);
    current_world->player.view_bob_screen_offset = vec3f::lerp(current_world->player.view_bob_screen_offset, target_view_bob_screen_offset, 0.035);
    player_pos = current_world->player.view_bob_offset + player_pos;
    camera.position = player_pos;
    camera.rot.x = xrot;
    camera.rot.y = yrot;
    player->rotation.x = xrot;
    player->rotation.y = yrot;

    current_world->update_frustum(camera);
}

void UpdateGUI(gertex::GXView &viewport)
{
    if (!gui::get_gui())
        return;

    for (input::device *dev : input::devices)
    {
        if (dev->connected())
        {
            // Update the cursor position based on the left stick
            vec3f left_stick = dev->get_left_stick();
            cursor_x += left_stick.x * 8;
            cursor_y -= left_stick.y * 8;

            // Override the cursor position with the pointer position if applicable
            if (dev->is_pointer_visible())
            {
                // Since you can technically go out of bounds using a joystick, we need to clamp the cursor position; it'd be really annoying losing the cursor
                cursor_x = std::clamp(cursor_x, 0, int(viewport.width / viewport.aspect_correction));
                cursor_y = std::clamp(cursor_y, 0, int(viewport.height));
            }
        }
    }
    gui::get_gui()->update();
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
    for (input::device *dev : input::devices)
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
    draw_textured_quad(icons_texture, (viewport.width - 364) / 2, corrected_height - 44, 364, 44, 56, 9, 238, 31);

    // Draw the hotbar selection
    draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + current_world->player.selected_hotbar_slot * 40 - 2, corrected_height - 46, 48, 48, 56, 31, 80, 55);
    // Push the orthogonal position matrix onto the stack
    gertex::push_matrix();

    // Draw the hotbar items
    for (size_t i = 0; i < 9; i++)
    {
        gui::draw_item((viewport.width - 364) / 2 + i * 40 + 6, corrected_height - 38, current_world->player.m_inventory[i], viewport);
    }

    // Restore the orthogonal position matrix
    gertex::pop_matrix();
    gertex::load_pos_matrix();

    // Enable direct colors as the previous call to draw_item may have changed the color mode
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw the player's health above the hotbar. The texture size is 9x9 but they overlap by 1 pixel.
    int health = current_world->player.m_entity->health;

    // Empty hearts
    for (int i = 0; i < 10; i++)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + i * 16, corrected_height - 64, 18, 18, 16, 0, 25, 9);
    }

    // Full hearts
    for (int i = 0; i < (health & ~1); i += 2)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + i * 8, corrected_height - 64, 18, 18, 52, 0, 61, 9);
    }

    // Half heart (if the player has an odd amount of health)
    if ((health & 1) == 1)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + (health & ~1) * 8, corrected_height - 64, 18, 18, 61, 0, 70, 9);
    }
}

void DrawGUI(gertex::GXView &viewport)
{
    gui *m_gui = gui::get_gui();
    if (!m_gui)
        return;

    // This is required as blocks require a dummy chunk to be rendered
    if (!get_chunks().size() && m_gui->use_cursor())
        return;

    // Disable depth testing for GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);

    // Reset the orthogonal position matrix and push it onto the stack
    gertex::ortho(viewport);
    gertex::push_matrix();

    // Draw the GUI elements
    m_gui->viewport = viewport;
    gui::get_gui()->draw();

    // Restore the orthogonal position matrix
    gertex::pop_matrix();
    gertex::load_pos_matrix();

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    if (!gui::get_gui()->use_cursor())
        return;

    bool pointer_visible = false;

    for (input::device *dev : input::devices)
    {
        if (dev->is_pointer_visible())
        {
            pointer_visible = true;
            cursor_x = dev->get_pointer_x();
            cursor_y = dev->get_pointer_y();
        }
    }

    // Draw the cursor
    if (pointer_visible && !gui::get_gui()->contains(cursor_x, cursor_y))
        draw_textured_quad(icons_texture, cursor_x - 7, cursor_y - 3, 32, 48, 32, 32, 48, 56);
    else
        draw_textured_quad(icons_texture, cursor_x - 16, cursor_y - 16, 32, 32, 0, 32, 32, 64);
}
void PrepareTEV()
{
    GX_SetNumTevStages(4);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    // -------- Stage 0: Texture * CLR0 --------

    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
    GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
    GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // -------- Stage 1: PREV * CLR1 --------

    GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP_DISABLE, GX_COLOR1A1);

    GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_RASC, GX_CC_ZERO);
    GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_RASA, GX_CA_ZERO);
    GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // -------- Stage 2: Add Konst0 --------

    GX_SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP_DISABLE, GX_COLOR0A0);

    GX_SetTevKColor(GX_KCOLOR0, (GXColor){0, 0, 0, 255}); // Default to black (i.e. no effect)
    GX_SetTevKColorSel(GX_TEVSTAGE2, GX_TEV_KCSEL_K0);

    GX_SetTevColorIn(GX_TEVSTAGE2, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
    GX_SetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    GX_SetTevAlphaIn(GX_TEVSTAGE2, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GX_SetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // -------- Stage 3: Multiply by Konst1 --------

    GX_SetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD0, GX_TEXMAP_DISABLE, GX_COLOR0A0);

    GX_SetTevKColor(GX_KCOLOR1, (GXColor){255, 255, 255, 255}); // White by default
    GX_SetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K1);

    GX_SetTevColorIn(GX_TEVSTAGE3, GX_CC_ZERO, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
    GX_SetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    GX_SetTevAlphaIn(GX_TEVSTAGE3, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GX_SetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}
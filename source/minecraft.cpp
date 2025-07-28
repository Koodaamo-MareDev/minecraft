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
#include "crapper/client.hpp"
#include "world.hpp"
#include "config.hpp"

#ifdef MONO_LIGHTING
#include "light_day_mono_rgba.h"
#include "light_night_mono_rgba.h"
#else
#include "light_day_rgba.h"
#include "light_night_rgba.h"
#endif

#define DEFAULT_FIFO_SIZE (256 * 1024)
#define DIRECTIONAL_LIGHT GX_ENABLE

void *frameBuffer[2] = {NULL, NULL};
static GXRModeObj *rmode = NULL;

f32 xrot = 0.0f;
f32 yrot = 0.0f;
guVector player_pos = {0.F, 80.0F, 0.F};

world *current_world = nullptr;

configuration config;

int frameCounter = 0;

u32 wiimote_down = 0;
u32 wiimote_held = 0;
float wiimote_x = 0;
float wiimote_z = 0;
float wiimote_rx = 0;
float wiimote_ry = 0;
float wiimote_ir_x = 0;
float wiimote_ir_y = 0;
bool wiimote_ir_visible = false;
u32 raw_wiimote_down = 0;
u32 raw_wiimote_held = 0;
vec3f left_stick(0, 0, 0);
vec3f right_stick(0, 0, 0);
int shoulder_btn_frame_counter = 0;
float prev_left_shoulder = 0;
float prev_right_shoulder = 0;
bool should_destroy_block = false;
bool should_place_block = false;

int cursor_x = 0;
int cursor_y = 0;

float fog_depth_multiplier = 1.0f;
float fog_light_multiplier = 1.0f;

Crapper::MinecraftClient client;
ByteBuffer receive_buffer;

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
void UpdateNetwork();
void UpdateLoadingStatus();
void UpdateLightDir();
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

float flerp(float a, float b, float f)
{
    return a + f * (b - a);
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
    catch (std::exception &e)
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
    GX_SetVtxDesc(GX_VA_NRM, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    // Prepare TEV
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0, DIRECTIONAL_LIGHT, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT0, GX_DF_CLAMP, GX_AF_SPOT);
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

    f32 FOV = config.get<float>("fov", 90.0f);

    camera_t camera = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}, // Camera position
        FOV,                // Field of view
        viewport.aspect,    // Aspect ratio
        viewport.near,      // Near clipping plane
        viewport.far        // Far clipping plane
    };
    current_world = new world;

    // Add the player to the world - it should persist until the game is closed
    current_world->player.m_entity = new entity_player_local(vec3f(0.5, -999, 0.5));
    add_entity(current_world->player.m_entity);

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

    std::string server_ip = config.get<std::string>("server", "");

    if (!server_ip.empty() && Crapper::initNetwork())
    {
        int32_t server_port = config.get<int32_t>("port", 25565);

        // Generate a "unique" username based on the device ID
        uint32_t dev_id = 0;
        ES_GetDeviceID(&dev_id);

        // Use the username from the config or generate a default one
        client.username = std::string(config.get<std::string>("username", "Wii_" + std::to_string(dev_id)));
        config["username"] = client.username;

        // Attempt to connect to the server
        client.joinServer(server_ip, server_port);

        if (client.status != Crapper::ErrorStatus::OK)
        {
            // Reset the world if the connection failed
            current_world->reset();
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
            dirtscreen->set_text("Saving world...");
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
            fog_light_multiplier = flerp(fog_light_multiplier, std::pow(0.9f, (15.0f - block->sky_light)), 0.05f);

        fog_depth_multiplier = flerp(fog_depth_multiplier, std::min(std::max(player_pos.y, 24.f) / 36.f, 1.0f), 0.05f);

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

        UpdateLightDir();

        GetInput();

        for (uint32_t i = current_world->last_entity_tick, count = 0; i < current_world->ticks && count < 10; i++, count++)
        {
            update_textures();

            UpdateNetwork();

            current_world->tick();

            wiimote_down = 0;
            wiimote_held = 0;
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
        for (int i = 0; i < rmode->efbHeight; i++)
        {
            memcpy((char *)frameBuffer[fb] + i * rmode->viWidth * VI_DISPLAY_PIX_SZ + (rmode->viWidth - 224) * VI_DISPLAY_PIX_SZ, (char *)frameBuffer[2] + i * rmode->viWidth * VI_DISPLAY_PIX_SZ, 224 * VI_DISPLAY_PIX_SZ);
        }
#endif
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
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
    if (client.status == Crapper::ErrorStatus::OK && current_world->is_remote())
    {
        client.disconnect();
    }
    current_world->save();
    current_world->reset();
    Crapper::deinitNetwork();
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
                dirtscreen->set_text("Loading level\n\n\nBuilding terrain");
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
    static u32 prev_nunchuk_held = 0;
    raw_wiimote_down = WPAD_ButtonsDown(0);
    raw_wiimote_held = WPAD_ButtonsHeld(0);
    if ((raw_wiimote_down & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME)))
        isExiting = true;
    if ((raw_wiimote_down & WPAD_BUTTON_1))
    {
        printf("SPEED: %f, %f\n", wiimote_x, wiimote_z);
        printf("POS: %f, %f, %f\n", player_pos.x, player_pos.y, player_pos.z);
    }

    expansion_t expansion;
    WPAD_Expansion(0, &expansion);
    left_stick = vec3f(0, 0, 0);
    right_stick = vec3f(0, 0, 0);
    static float sensitivity = config.get("look_sensitivity", 360.0f);
    static float deadzone = std::min(float(config.get("joystick_deadzone", 0.1f)), 0.5f);
    if (expansion.type == WPAD_EXP_NONE || expansion.type == WPAD_EXP_UNKNOWN)
    {
        return;
    }
    else if (expansion.type == WPAD_EXP_NUNCHUK)
    {
        static int ir_tracking_keep_alive = 0;
        if (ir_tracking_keep_alive % 300 == 0)
        {
            // Enable IR tracking every 5 seconds to prevent tracking loss due to connection issues
            ir_tracking_keep_alive = 0;
            WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
        }
        ir_tracking_keep_alive++;

        u32 nunchuk_held = expansion.nunchuk.btns_held;
        u32 nunchuk_down = expansion.nunchuk.btns_held & ~prev_nunchuk_held;
        prev_nunchuk_held = nunchuk_held;

        u32 new_raw_wiimote_down = 0;
        u32 new_raw_wiimote_held = 0;

        // Emulate the right stick with the IR sensor
        ir_t ir;
        WPAD_IR(0, &ir);
        if (ir.valid)
        {
            wiimote_ir_visible = true;
            right_stick.x = (ir.x / ir.vres[0]) - 0.5;
            right_stick.y = 0.5 - (ir.y / ir.vres[1]);

            wiimote_ir_x = ir.x / ir.vres[0];
            wiimote_ir_y = ir.y / ir.vres[1];

            // Ensure that the coordinates are at least 0.125 away from the center to prevent accidental movement.
            if (right_stick.magnitude() < 0.125)
            {
                right_stick = vec3f(0, 0, 0);
            }
            else
            {
                // Magnitude of 0.25 or less means that the stick is near the center. 1 means it's at the edge and means it will move at full speed.
                right_stick = right_stick.fast_normalize() * ((right_stick.magnitude() - 0.125) / 0.875);

                // Multiply the result by 2 since the edges of the screen are difficult to reach as tracking is lost.
                right_stick = right_stick * 2;
            }
        }
        else
        {
            wiimote_ir_visible = false;
        }

        if (right_stick.magnitude() > 1.0)
            right_stick = right_stick.fast_normalize();

        // Map the Wiimote and Nunchuck buttons to the Classic Controller buttons
        if ((nunchuk_held & NUNCHUK_BUTTON_C))
            new_raw_wiimote_held |= WPAD_CLASSIC_BUTTON_X;
        if ((raw_wiimote_held & WPAD_BUTTON_A))
            new_raw_wiimote_held |= WPAD_CLASSIC_BUTTON_B;
        if ((nunchuk_held & NUNCHUK_BUTTON_Z))
            new_raw_wiimote_held |= WPAD_CLASSIC_BUTTON_FULL_L;
        if ((raw_wiimote_held & WPAD_BUTTON_B))
            new_raw_wiimote_held |= WPAD_CLASSIC_BUTTON_FULL_R;
        if ((raw_wiimote_held & WPAD_BUTTON_MINUS))
            new_raw_wiimote_held |= WPAD_CLASSIC_BUTTON_ZL;
        if ((raw_wiimote_held & WPAD_BUTTON_PLUS))
            new_raw_wiimote_held |= WPAD_CLASSIC_BUTTON_ZR;

        if ((nunchuk_down & NUNCHUK_BUTTON_C))
            new_raw_wiimote_down |= WPAD_CLASSIC_BUTTON_X;
        if ((raw_wiimote_down & WPAD_BUTTON_A))
            new_raw_wiimote_down |= WPAD_CLASSIC_BUTTON_B;
        if ((nunchuk_down & NUNCHUK_BUTTON_Z))
            new_raw_wiimote_down |= WPAD_CLASSIC_BUTTON_FULL_L;
        if ((raw_wiimote_down & WPAD_BUTTON_B))
            new_raw_wiimote_down |= WPAD_CLASSIC_BUTTON_FULL_R;
        if ((raw_wiimote_down & WPAD_BUTTON_MINUS))
            new_raw_wiimote_down |= WPAD_CLASSIC_BUTTON_ZL;
        if ((raw_wiimote_down & WPAD_BUTTON_PLUS))
            new_raw_wiimote_down |= WPAD_CLASSIC_BUTTON_ZR;

        raw_wiimote_down = new_raw_wiimote_down;
        raw_wiimote_held = new_raw_wiimote_held;
        left_stick.x = float(int(expansion.nunchuk.js.pos.x) - int(expansion.nunchuk.js.center.x)) * 2 / (int(expansion.nunchuk.js.max.x) - 2 - int(expansion.nunchuk.js.min.x));
        left_stick.y = float(int(expansion.nunchuk.js.pos.y) - int(expansion.nunchuk.js.center.y)) * 2 / (int(expansion.nunchuk.js.max.y) - 2 - int(expansion.nunchuk.js.min.y));
    }
    else if (expansion.type == WPAD_EXP_CLASSIC)
    {
        right_stick.x = float(int(expansion.classic.rjs.pos.x) - int(expansion.classic.rjs.center.x)) * 2 / (int(expansion.classic.rjs.max.x) - 2 - int(expansion.classic.rjs.min.x));
        right_stick.y = float(int(expansion.classic.rjs.pos.y) - int(expansion.classic.rjs.center.y)) * 2 / (int(expansion.classic.rjs.max.y) - 2 - int(expansion.classic.rjs.min.y));
        left_stick.x = float(int(expansion.classic.ljs.pos.x) - int(expansion.classic.ljs.center.x)) * 2 / (int(expansion.classic.ljs.max.x) - 2 - int(expansion.classic.ljs.min.x));
        left_stick.y = float(int(expansion.classic.ljs.pos.y) - int(expansion.classic.ljs.center.y)) * 2 / (int(expansion.classic.ljs.max.y) - 2 - int(expansion.classic.ljs.min.y));
    }

    if (left_stick.magnitude() < deadzone)
    {
        left_stick.x = 0;
        left_stick.y = 0;
    }

    if (right_stick.magnitude() < deadzone)
    {
        right_stick.x = 0;
        right_stick.y = 0;
    }

    if (!gui::get_gui())
    {
        float target_x = left_stick.x * sin(DegToRad(yrot + 90));
        float target_z = left_stick.x * cos(DegToRad(yrot + 90));
        target_x -= left_stick.y * sin(DegToRad(yrot));
        target_z -= left_stick.y * cos(DegToRad(yrot));

        if (current_world)
        {
            yrot -= right_stick.x * current_world->delta_time * sensitivity;
            xrot += right_stick.y * current_world->delta_time * sensitivity;

            if (yrot > 360.f)
                yrot -= 360.f;
            if (yrot < 0)
                yrot += 360.f;
            if (xrot > 90.f)
                xrot = 90.f;
            if (xrot < -90.f)
                xrot = -90.f;
        }

        wiimote_x = target_x;
        wiimote_z = target_z;

        wiimote_down |= raw_wiimote_down;
        wiimote_held |= raw_wiimote_held;

        wiimote_rx = right_stick.x;
        wiimote_ry = right_stick.y;

        if (!((raw_wiimote_held & WPAD_CLASSIC_BUTTON_FULL_L) || (raw_wiimote_held & WPAD_CLASSIC_BUTTON_FULL_R)))
            shoulder_btn_frame_counter = -1;
        else
            shoulder_btn_frame_counter++;

        should_place_block = false;
        should_destroy_block = false;
        if (shoulder_btn_frame_counter >= 0)
        {
            // repeats buttons every 10 frames
            if ((raw_wiimote_held & WPAD_CLASSIC_BUTTON_FULL_L) && ((raw_wiimote_down & WPAD_CLASSIC_BUTTON_FULL_L) || shoulder_btn_frame_counter % 10 == 0))
                should_place_block = true;
            if ((raw_wiimote_held & WPAD_CLASSIC_BUTTON_FULL_R))
                should_place_block = !(should_destroy_block = true);
        }
        if (raw_wiimote_down & WPAD_CLASSIC_BUTTON_ZL)
        {
            current_world->player.selected_hotbar_slot = (current_world->player.selected_hotbar_slot + 8) % 9;
            if (current_world->is_remote())
                client.sendBlockItemSwitch(current_world->player.selected_hotbar_slot);
        }
        if (raw_wiimote_down & WPAD_CLASSIC_BUTTON_ZR)
        {
            current_world->player.selected_hotbar_slot = (current_world->player.selected_hotbar_slot + 1) % 9;
            if (current_world->is_remote())
                client.sendBlockItemSwitch(current_world->player.selected_hotbar_slot);
        }
    } // If the inventory is visible, don't allow the player to move
    else
    {
        wiimote_x = 0;
        wiimote_z = 0;
        should_place_block = false;
        should_destroy_block = false;
    }
}

// TODO: Finish fake lighting
void UpdateLightDir()
{
    if (!DIRECTIONAL_LIGHT)
        return;
    GXLightObj light;
    guVector look_dir{0, 1, 0};
    GX_InitLightPos(&light, look_dir.x * -8192, look_dir.y * -8192, look_dir.z * -8192);
    GX_InitLightColor(&light, GXColor{255, 255, 255, 255});
    GX_InitLightAttnA(&light, 1.0, 0.0, 0.0);
    GX_InitLightDistAttn(&light, 1.0, 1.0, GX_DA_OFF);
    GX_InitLightDir(&light, look_dir.x, look_dir.y, look_dir.z);
    GX_LoadLightObj(&light, GX_LIGHT0);
}

void HandleGUI(gertex::GXView &viewport)
{
    if ((raw_wiimote_down & WPAD_CLASSIC_BUTTON_X) != 0)
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
    if (current_world->loaded)
    {
        // Draw the underwater overlay
        static vec3f pan_underwater_texture(0, 0, 0);
        pan_underwater_texture = pan_underwater_texture + vec3f(wiimote_rx * 0.25, wiimote_ry * 0.25, 0.0);
        float corrected_height = viewport.height * viewport.aspect_correction;
        pan_underwater_texture.x = std::fmod(pan_underwater_texture.x, viewport.width);
        pan_underwater_texture.y = std::fmod(pan_underwater_texture.y, corrected_height);
        static float vignette_strength = 0;
        vignette_strength = flerp(vignette_strength, 1.0f - (current_world->player.m_entity->light_level) / 15.0f, 0.01f);
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

    // Display debug information
    gui::draw_text_with_shadow(0, viewport.ystart, "Memory: " + std::to_string(current_world->memory_usage) + " B");
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

void UpdateNetwork()
{
    if (client.status != Crapper::ErrorStatus::OK || !current_world->is_remote())
        return;
    // Receive packets
    client.receive(receive_buffer);

    // Handle up to 100 packets
    for (int i = 0; i < 100 && client.status == Crapper::ErrorStatus::OK; i++)
    {
        if (client.handlePacket(receive_buffer))
            break;
    }

    // Check for errors
    // NOTE: Won't run if the client is disconnected before the handshake.
    if (client.status != Crapper::ErrorStatus::OK)
    {
        // Reset the world if the connection is lost
        current_world->reset();
        return;
    }
    if (!client.login_success)
        return;

    // Send keep alive every 2700 frames (every 45 seconds)
    if (current_world->ticks % 900 == 0)
        client.sendKeepAlive();

    // Send the player's position and look every 3 ticks
    if (current_world->ticks % 3 == 0 && frameCounter != 0)
        client.sendPlayerPositionLook();

    // Send the player's grounded status if it has changed
    bool on_ground = current_world->player.m_entity->on_ground;
    if (frameCounter > 0 && client.on_ground != on_ground)
        client.sendGrounded(on_ground);
}

void UpdateGUI(gertex::GXView &viewport)
{
    if (!gui::get_gui())
        return;

    cursor_x += left_stick.x * 8;
    cursor_y -= left_stick.y * 8;

    if (!wiimote_ir_visible)
    {
        // Since you can technically go out of bounds using a joystick, we need to clamp the cursor position; it'd be really annoying losing the cursor
        cursor_x = std::clamp(cursor_x, 0, int(viewport.width / viewport.aspect_correction));
        cursor_y = std::clamp(cursor_y, 0, int(viewport.height));
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

    // Draw IR cursor if visible
    if (wiimote_ir_visible)
    {
        draw_textured_quad(icons_texture, wiimote_ir_x * viewport.width - 7, wiimote_ir_y * corrected_height - 3, 48, 48, 32, 32, 56, 56);
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

    // Draw the cursor
    if (wiimote_ir_visible && !gui::get_gui()->contains(cursor_x, cursor_y))
        draw_textured_quad(icons_texture, cursor_x - 7, cursor_y - 3, 32, 48, 32, 32, 48, 56);
    else
        draw_textured_quad(icons_texture, cursor_x - 16, cursor_y - 16, 32, 32, 0, 32, 32, 64);
}

void PrepareTEV()
{

    GX_SetNumTevStages(3);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    // Stage 0 handles the basics like texture mapping and lighting

    // Set the TEV stage 0 to use the texture coordinates and the texture map
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    // Set the alpha sources to the texture alpha and the rasterized alpha
    GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
    GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // Set the color sources to the texture color and the rasterized color
    GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
    GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // Stage 1 handles blending the texture with a constant color additively

    // Set the TEV stage 1 to use the texture coordinates and the texture map
    GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    // Keep the alpha as it was from the previous stage
    GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // Set the constant color to black
    GX_SetTevKColor(GX_KCOLOR0, (GXColor){0, 0, 0, 255});
    GX_SetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K0);

    // Additive blending
    GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
    GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // Stage 2 handles the final color output by multiplying the stage 1 color with the constant color

    // Set the TEV stage 2 to use the texture coordinates and the texture map
    GX_SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    // Keep the alpha as it was from the previous stage
    GX_SetTevAlphaIn(GX_TEVSTAGE2, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GX_SetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // Set the constant color to white
    GX_SetTevKColor(GX_KCOLOR1, (GXColor){255, 255, 255, 255});
    GX_SetTevKColorSel(GX_TEVSTAGE2, GX_TEV_KCSEL_K1);

    // Multiply the color from the previous stage with the constant color
    GX_SetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
    GX_SetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}
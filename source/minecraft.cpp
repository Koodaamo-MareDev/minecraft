#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <vector>
#include <algorithm>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include <fat.h>
#include <ogc/conf.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "mcregion.hpp"
#include "nbt/nbt.hpp"
#include "chunk_new.hpp"
#include "block.hpp"
#include "blocks.hpp"
#include "brightness_values.h"
#include "vec3i.hpp"
#include "timers.hpp"
#include "texturedefs.h"
#include "raycast.hpp"
#include "light.hpp"
#include "texanim.hpp"
#include "base3d.hpp"
#include "render.hpp"
#include "render_gui.hpp"
#include "lock.hpp"
#include "asynclib.hpp"
#include "particle.hpp"
#include "sound.hpp"
#include "inventory.hpp"
#include "gui.hpp"
#include "gui_survival.hpp"
#include "crapper/client.hpp"

#ifdef MONO_LIGHTING
#include "light_day_mono_rgba.h"
#include "light_night_mono_rgba.h"
#else
#include "light_day_rgba.h"
#include "light_night_rgba.h"
#endif

#define DEFAULT_FIFO_SIZE (256 * 1024)
#define CLASSIC_CONTROLLER_THRESHOLD 4
#define MAX_PARTICLES 100
#define LOOKAROUND_SENSITIVITY 180.0f
#define MOVEMENT_SPEED 15
#define JUMP_HEIGHT 1.25f
#define DIRECTIONAL_LIGHT GX_ENABLE
// #define NO_LOADING_SCREEN

f32 xrot = 0.0f;
f32 yrot = 0.0f;
aabb_entity_t *player = nullptr;
guVector player_pos = {0.F, 80.0F, 0.F};
void *frameBuffer[3] = {NULL, NULL, NULL};
static GXRModeObj *rmode = NULL;

bool draw_block_outline = false;
vec3i raycast_pos;
vec3i raycast_face;
std::vector<aabb_t> block_bounds;

int frameCounter = 0;
int tickCounter = 0;
int timeOfDay = 0;
int lastEntityTick = 0;
int lastWaterTick = 0;
int fluidUpdateCount = 0;
float lastStepDistance = 0;
double deltaTime = 0.0;
double partialTicks = 0.0;

std::string world_name = "world";

uint32_t total_chunks_size = 0;

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
bool destroy_block = false;
bool place_block = false;
int selected_hotbar_slot = 0;
inventory::item_stack *selected_item = nullptr;

vec3f view_bob_offset(0, 0, 0);
vec3f view_bob_screen_offset(0, 0, 0);

particle_system_t particle_system;
sound_system_t *sound_system = nullptr;

int cursor_x = 0;
int cursor_y = 0;
bool show_dirtscreen = true;
bool has_loaded = false;

gui *current_gui = nullptr;
inventory::container player_inventory(40, 36); // 4 rows of 9 slots, the rest 4 are the armor slots

float fog_depth_multiplier = 1.0f;
float fog_light_multiplier = 1.0f;
BlockID in_fluid = BlockID::air;

Crapper::MinecraftClient client;
Crapper::ByteBuffer receive_buffer;
std::string dirtscreen_text = "Loading...";

void dbgprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    // Immediately display the output
#ifdef DEBUG
    void *fb = VIDEO_GetNextFramebuffer();
    for (int i = 0; i < rmode->efbHeight; i++)
    {
        memcpy((char *)fb + i * rmode->viWidth * VI_DISPLAY_PIX_SZ + (rmode->viWidth - 224) * VI_DISPLAY_PIX_SZ, (char *)frameBuffer[2] + i * rmode->viWidth * VI_DISPLAY_PIX_SZ, 224 * VI_DISPLAY_PIX_SZ);
    }
    VIDEO_Flush();
#endif
}

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
void SaveWorld();
bool LoadWorld();
void ResetWorld();
void UpdateNetwork();
void DestroyBlock(const vec3i &pos, block_t old_block);
void TryPlaceBlock(const vec3i &pos, const vec3i &targeted, block_t new_block, uint8_t face);
void SpawnDrop(const vec3i &pos, const block_t &old_block, inventory::item_stack item);
void CreateExplosion(vec3f pos, float power, chunk_t *near);
void UpdateLoadingStatus();
void UpdateLightDir();
void HandleGUI(gertex::GXView &viewport);
void UpdateInventory(gertex::GXView &viewport);
void DrawInventory(gertex::GXView &viewport);
void DrawHUD(gertex::GXView &viewport);
void DrawSelectedBlock();
void DrawScene(bool transparency);
void GenerateChunks(int count);
void RemoveRedundantChunks();
void PrepareChunkRemoval(chunk_t *chunk);
void UpdateCamera(camera_t &camera);
void UpdateChunkData(frustum_t &frustum, std::deque<chunk_t *> &chunks);
void UpdatePlayer();
void UpdateScene(frustum_t &frustum);
void UpdateChunkVBOs(std::deque<chunk_t *> &chunks);
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

s8 has_retraced = 0;
void RenderDone(u32 enterCount)
{
    has_retraced = 1;
}
uint8_t light_map[1024] ATTRIBUTE_ALIGN(32) = {0};

GXColor ColorBlend(GXColor color0, GXColor color1, float factor)
{
    float inv_factor = 1.0f - factor;
    return GXColor{uint8_t((color0.r * inv_factor) + (color1.r * factor)),
                   uint8_t((color0.g * inv_factor) + (color1.g * factor)),
                   uint8_t((color0.b * inv_factor) + (color1.b * factor)),
                   uint8_t((color0.a * inv_factor) + (color1.a * factor))};
}
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

void DrawOutline(aabb_t &aabb)
{
    GX_BeginGroup(GX_LINES, 24);

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

int main(int argc, char **argv)
{
    u32 fb = 0;
    f32 yscale;
    u32 xfbHeight;
    void *gpfifo = NULL;
    GXColor background = get_sky_color();

    async_lib::init();
    VIDEO_Init();
    WPAD_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    SYS_SetPowerCallback(WiiPowerPressed);
    SYS_SetResetCallback(WiiResetPressed);
    WPAD_SetPowerButtonCallback(WiimotePowerPressed);

    // Allocate the fifo buffer
    gpfifo = memalign(32, DEFAULT_FIFO_SIZE);
    memset(gpfifo, 0, DEFAULT_FIFO_SIZE);

    // Allocate 2 framebuffers for double buffering
    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

#ifdef DEBUG
    frameBuffer[2] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    CON_Init(frameBuffer[2], 0, 0, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
#endif
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
    cursor_y = viewport.height / 2;

    f32 FOV = 90;

    camera_t camera = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}, // Camera position
        FOV,                // Field of view
        viewport.aspect,    // Aspect ratio
        viewport.near,      // Near clipping plane
        viewport.far        // Far clipping plane
    };
    fatInitDefault();
    std::string save_path = "/apps/minecraft/saves/" + world_name;
    std::string region_path = save_path + "/region";
    mkpath(region_path.c_str(), 0777);
    chdir(save_path.c_str());

    printf("Render resolution: %f,%f, Widescreen: %s\n", viewport.width, viewport.height, viewport.widescreen ? "Yes" : "No");
    light_engine_init();
    init_chunks();
    srand(world_seed = gettime());
    printf("Initialized chunks.\n");
    VIDEO_Flush();
    VIDEO_WaitVSync();
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetZCompLoc(GX_FALSE);
    GX_SetLineWidth(16, GX_VTXFMT0);
    player_pos.x = 0;
    player_pos.y = -1000;
    player_pos.z = 0;
    VIDEO_SetPostRetraceCallback(&RenderDone);
    init_face_normals();
    PrepareTEV();
    std::deque<chunk_t *> &chunks = get_chunks();

    sound_system = new sound_system_t();
    inventory::init_items();
    gui::init_matrices();

    selected_item = &player_inventory[0];

    // Add the player to the world - it should persist until the game is closed
    player = new aabb_entity_t(0.6, 1.8);
    player->local = true;
    player->y_offset = 1.62;
    player->y_size = 0;
    player->teleport(vec3f(0.5, -999, 0.5));
    add_entity(player);

    gertex::GXFog fog = gertex::GXFog{true, gertex::GXFogType::linear, viewport.near, viewport.far, viewport.near, viewport.far, background};
#ifdef MULTIPLAYER
    if (Crapper::initNetwork())
    {
        uint32_t dev_id = 0;
        ES_GetDeviceID(&dev_id);
        client.username = "Wii_" + std::to_string(dev_id);

        client.joinServer("desktop-marcus.local", 25566);
        if (client.status != Crapper::ErrorStatus::OK)
            client.joinServer("mc.okayu.zip", 25566);
        if (client.status == Crapper::ErrorStatus::OK)
        {
            // Display status message
            dirtscreen_text = "Logging in...";
        }
        else
        {
            // Reset the world if the connection failed
            ResetWorld();
        }
    }
#endif
    if (!is_remote())
    {
        if (!LoadWorld())
        {
            printf("Failed to load world, creating new world...\n");
            ResetWorld();
        }
    }

    GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));

    while (!isExiting)
    {
        u64 frame_start = time_get();
        float sky_multiplier = get_sky_multiplier();
        if (HWButton != -1)
            break;
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
        if (!is_hell_world())
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

        block_t *block = get_block_at(vec3i(std::floor(player->position.x), std::floor(player->aabb.min.y + player->y_offset), std::floor(player->position.z)));
        if (block)
            fog_light_multiplier = flerp(fog_light_multiplier, std::pow(0.9f, (15.0f - block->sky_light)), 0.05f);

        fog_depth_multiplier = flerp(fog_depth_multiplier, std::min(std::max(player_pos.y, 24.f) / 36.f, 1.0f), 0.05f);

        float fog_multiplier = is_hell_world() ? 0.5f : 1.0f;
        if (in_fluid == BlockID::lava)
        {
            background = GXColor{0xFF, 0, 0, 0xFF};
            fog_multiplier = 0.05f;
        }
        else if (in_fluid == BlockID::water)
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

        UpdateNetwork();

        GetInput();

        for (int i = lastEntityTick, count = 0; i < tickCounter && count < 10; i++, count++)
        {
            world_tick++;

            update_textures();

            // Find a chunk for any lingering entities
            std::map<int32_t, aabb_entity_t *> &world_entities = get_entities();
            for (auto &&e : world_entities)
            {
                aabb_entity_t *entity = e.second;
                if (!entity->chunk)
                {
                    vec3i int_pos = vec3i(int(std::floor(entity->position.x)), 0, int(std::floor(entity->position.z)));
                    entity->chunk = get_chunk_from_pos(int_pos);
                    if (entity->chunk)
                        entity->chunk->entities.push_back(entity);
                }
            }

            if (has_loaded)
            {
                // Update the entities of the world
                for (chunk_t *&chunk : chunks)
                {
                    // Update the entities in the chunk
                    chunk->update_entities();
                }

                // FIXME: This is a hacky fix for player movement not working in unloaded chunks
                if (!player->chunk && !show_dirtscreen)
                    player->tick();

                // Update the player entity
                UpdatePlayer();
                for (auto &&e : world_entities)
                {
                    aabb_entity_t *entity = e.second;
                    if (!entity || entity->dead)
                        continue;
                    // Tick the entity animations
                    entity->animate();
                }
            }

            wiimote_down = 0;
            wiimote_held = 0;
        }
        lastEntityTick = tickCounter;

        // Update the sound system
        sound_system->update(angles_to_vector(0, yrot + 90), player->get_position(std::fmod(partialTicks, 1)));

        UpdateCamera(camera);

        // Wrap the player around the world
        if (!is_remote() && player->aabb.min.y < -750)
            player->teleport(vec3f(player->position.x, 256, player->position.z));

        // Construct the view frustum matrix from the camera
        frustum_t frustum = calculate_frustum(camera);

        UpdateScene(frustum);

        UpdateLoadingStatus();

        gertex::perspective(viewport);

        // Set fog near and far distances
        fog.start = fog_multiplier * fog_depth_multiplier * (GENERATION_DISTANCE - is_remote()) * 8.0f;
        fog.end = fog_multiplier * fog_depth_multiplier * (GENERATION_DISTANCE - is_remote()) * 16.0f;

        // Enable fog
        gertex::set_fog(fog);

        if (!show_dirtscreen)
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
            GX_SetAlphaCompare(GX_GEQUAL, 1, GX_AOP_AND, GX_ALWAYS, 0);
            draw_particles(camera, particle_system.particles, particle_system.size());

            // Draw chunks
            GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
            DrawScene(false);

            // Prepare transparent rendering parameters
            GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
            gertex::set_blending(gertex::GXBlendMode::normal);
            GX_SetAlphaUpdate(GX_FALSE);

            // Draw chunks
            GX_SetAlphaCompare(GX_GEQUAL, 1, GX_AOP_AND, GX_ALWAYS, 0);
            DrawScene(true);

            // Draw selected block
            DrawSelectedBlock();

            // Draw sky
            if (in_fluid == BlockID::air && !is_hell_world())
                draw_sky(background);
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
        deltaTime = time_diff_s(frame_start, time_get());

        // Ensure that the delta time is not too large to prevent issues
        if (deltaTime > 0.05)
            deltaTime = 0.05;

        partialTicks += deltaTime * 20.0;
        timeOfDay += int(partialTicks);
        timeOfDay %= 24000;
        tickCounter += int(partialTicks);
        partialTicks -= int(partialTicks);
        fb ^= 1;
    }
    if (client.status == Crapper::ErrorStatus::OK)
    {
        client.disconnect();
    }
    SaveWorld();
    ResetWorld();
    printf("De-initializing network...\n");
    Crapper::deinitNetwork();
    printf("De-initializing light engine...\n");
    light_engine_deinit();
    printf("De-initializing chunk engine...\n");
    deinit_chunks();
    printf("De-initializing sound system...\n");
    delete sound_system;
    printf("Exiting...");
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
#ifdef NO_LOADING_SCREEN
    has_loaded = true;
#endif
    bool is_loading = false;
    if (!has_loaded)
    {
        // Check if a 3x3 chunk area around the player is loaded
        vec3i player_chunk_pos = vec3i(int(player_pos.x), int(player_pos.y), int(player_pos.z));
        for (int x = -1; x <= 1 && !is_loading; x++)
        {
            for (int z = -1; z <= 1 && !is_loading; z++)
            {
                vec3i chunk_pos = player_chunk_pos + vec3i(x * 16, 0, z * 16);
                chunk_t *chunk = get_chunk_from_pos(chunk_pos);
                if (!chunk || chunk->generation_stage != ChunkGenStage::done)
                {
                    is_loading = true;
                    break;
                }
                // Check if the vbos near the player are visible and dirty
                for (int i = (player_chunk_pos.y / 16) - 1; i < (player_chunk_pos.y / 16) + 1; i++)
                {
                    if (chunk->vbos[i].visible && (chunk->vbos[i].dirty || chunk->vbos[i].cached_solid != chunk->vbos[i].solid || chunk->vbos[i].cached_transparent != chunk->vbos[i].transparent))
                    {
                        is_loading = true;
                        break;
                    }
                }
            }
        }
        if (!is_loading)
        {
            has_loaded = true;
        }
    }
    show_dirtscreen = !has_loaded;
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
        printf("PRIM_CHUNK_MEMORY: %d B\n", total_chunks_size);
        printf("TICK: %d, WATER: %d\n", tickCounter, lastWaterTick);
        printf("SPEED: %f, %f\n", wiimote_x, wiimote_z);
        printf("POS: %f, %f, %f\n", player_pos.x, player_pos.y, player_pos.z);
    }
    if ((raw_wiimote_down & WPAD_BUTTON_2))
    {
        tickCounter += 6000;
    }

    expansion_t expansion;
    WPAD_Expansion(0, &expansion);
    left_stick = vec3f(0, 0, 0);
    right_stick = vec3f(0, 0, 0);
    float sensitivity = LOOKAROUND_SENSITIVITY;
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
                right_stick = right_stick.normalize() * ((right_stick.magnitude() - 0.125) / 0.875);

                // Multiply the result by 2 since the edges of the screen are difficult to reach as tracking is lost.
                right_stick = right_stick * 2;
            }
        }
        else
        {
            wiimote_ir_visible = false;
        }

        if (right_stick.magnitude() > 1.0)
            right_stick = right_stick.normalize();

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

    if (left_stick.magnitude() < 0.1)
    {
        left_stick.x = 0;
        left_stick.y = 0;
    }

    if (right_stick.magnitude() < 0.1)
    {
        right_stick.x = 0;
        right_stick.y = 0;
    }

    if (!current_gui)
    {
        float target_x = left_stick.x * sin(DegToRad(yrot + 90));
        float target_z = left_stick.x * cos(DegToRad(yrot + 90));
        target_x -= left_stick.y * sin(DegToRad(yrot));
        target_z -= left_stick.y * cos(DegToRad(yrot));

        yrot -= right_stick.x * deltaTime * sensitivity;
        xrot += right_stick.y * deltaTime * sensitivity;

        if (yrot > 360.f)
            yrot -= 360.f;
        if (yrot < 0)
            yrot += 360.f;
        if (xrot > 90.f)
            xrot = 90.f;
        if (xrot < -90.f)
            xrot = -90.f;

        wiimote_x = target_x;
        wiimote_z = target_z;

        wiimote_down |= raw_wiimote_down;
        wiimote_held |= raw_wiimote_held;

        wiimote_rx = right_stick.x;
        wiimote_ry = right_stick.y;
    } // If the inventory is visible, don't allow the player to move
    else
    {
        wiimote_x = 0;
        wiimote_z = 0;
        return;
    }

    if (!((raw_wiimote_held & WPAD_CLASSIC_BUTTON_FULL_L) || (raw_wiimote_held & WPAD_CLASSIC_BUTTON_FULL_R)))
        shoulder_btn_frame_counter = -1;
    else
        shoulder_btn_frame_counter++;

    place_block = false;
    destroy_block = false;
    if (shoulder_btn_frame_counter >= 0)
    {
        // repeats buttons every 10 frames
        if ((raw_wiimote_held & WPAD_CLASSIC_BUTTON_FULL_L) && ((raw_wiimote_down & WPAD_CLASSIC_BUTTON_FULL_L) || shoulder_btn_frame_counter % 10 == 0))
            place_block = true;
        if ((raw_wiimote_held & WPAD_CLASSIC_BUTTON_FULL_R) && ((raw_wiimote_down & WPAD_CLASSIC_BUTTON_FULL_R) || shoulder_btn_frame_counter % 10 == 0))
            place_block = !(destroy_block = true);
    }
    if (raw_wiimote_down & WPAD_CLASSIC_BUTTON_ZL)
    {
        selected_hotbar_slot = (selected_hotbar_slot + 8) % 9;
        if (is_remote())
            client.sendBlockItemSwitch(selected_hotbar_slot);
    }
    if (raw_wiimote_down & WPAD_CLASSIC_BUTTON_ZR)
    {
        selected_hotbar_slot = (selected_hotbar_slot + 1) % 9;
        if (is_remote())
            client.sendBlockItemSwitch(selected_hotbar_slot);
    }

    if ((raw_wiimote_down & WPAD_CLASSIC_BUTTON_LEFT))
    {
        if (!is_remote())
        {
            vec3i block_pos;
            vec3i face;
            if (raycast(vec3f(player_pos.x + .5, player_pos.y + .5, player_pos.z + .5), angles_to_vector(xrot, yrot), 128, &block_pos, &face))
            {
                block_pos = block_pos + face;
                vec3f pos = vec3f(block_pos.x, block_pos.y, block_pos.z) + vec3f(0.5, 0.5, 0.5);
                CreateExplosion(pos, 3, get_chunk_from_pos(block_pos));
            }
        }
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
        if (current_gui)
        {
            current_gui->close();
            delete current_gui;
            current_gui = nullptr;
        }
        else
        {
            current_gui = new gui_survival(viewport, player_inventory);
        }
    }
    if (show_dirtscreen)
    {
        // Fill the screen with the dirt texture
        int texture_index = get_default_texture_index(BlockID::dirt);
        fill_screen_texture(blockmap_texture, viewport, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));

        // Draw the status text
        gui::draw_text((viewport.width - gui::text_width(dirtscreen_text)) / 2, viewport.height / 2, dirtscreen_text, GXColor{255, 255, 255, 255});
    }
    else
    {
        // Draw the underwater overlay
        static vec3f pan_underwater_texture(0, 0, 0);
        pan_underwater_texture = pan_underwater_texture + vec3f(wiimote_rx * 0.25, wiimote_ry * 0.25, 0.0);
        pan_underwater_texture.x = std::fmod(pan_underwater_texture.x, viewport.width);
        pan_underwater_texture.y = std::fmod(pan_underwater_texture.y, viewport.height);
        if (in_fluid == BlockID::water)
        {
            draw_textured_quad(underwater_texture, pan_underwater_texture.x - viewport.width, pan_underwater_texture.y - viewport.height, viewport.width * 3, viewport.height * 3, 0, 0, 48, 48);
            draw_textured_quad(vignette_texture, 0, 0, viewport.width, viewport.height, 0, 0, 256, 256);
        }
        draw_textured_quad(vignette_texture, 0, 0, viewport.width, viewport.height, 0, 0, 256, 256);

        DrawHUD(viewport);
        UpdateInventory(viewport);
        DrawInventory(viewport);
    }
}

void Render(guVector chunkPos, void *buffer, u32 length)
{
    if (!buffer || !length)
        return;
    transform_view(gertex::get_view_matrix(), chunkPos);
    // Render
    GX_CallDispList(buffer, length); // Draw the box
}

inline void RecalcSectionWater(chunk_t *chunk, int section)
{
    static std::vector<std::vector<vec3i>> fluid_levels(8);
    static bool init = false;
    if (!init)
    {
        for (size_t i = 0; i < fluid_levels.size(); i++)
        {
            std::vector<vec3i> &positions = fluid_levels[i];
            positions.reserve(4096);
        }
        init = true;
    }

    int chunkX = (chunk->x * 16);
    int chunkZ = (chunk->z * 16);
    int sectionY = (section * 16);
    vec3i current_pos = vec3i(chunkX, sectionY, chunkZ);
    current_pos.x = chunkX;
    for (int _x = 0; _x < 16; _x++, current_pos.x++)
    {
        current_pos.z = chunkZ;
        for (int _z = 0; _z < 16; _z++, current_pos.z++)
        {
            current_pos.y = sectionY;
            for (int _y = 0; _y < 16; _y++, current_pos.y++)
            {
                block_t *block = chunk->get_block(current_pos);
                if ((block->meta & FLUID_UPDATE_REQUIRED_FLAG))
                    fluid_levels[get_fluid_meta_level(block) & 7].push_back(current_pos);
            }
        }
    }
    uint16_t curr_fluid_count = 0;
    for (size_t i = 0; i < fluid_levels.size(); i++)
    {
        std::vector<vec3i> &positions = fluid_levels[i];
        for (vec3i pos : positions)
        {
            block_t *block = chunk->get_block(pos);
            if ((basefluid(block->get_blockid()) != BlockID::lava || fluidUpdateCount % 6 == 0))
                update_fluid(block, pos);
            curr_fluid_count++;
        }
        positions.clear();
    }
    chunk->has_fluid_updates[section] = (curr_fluid_count != 0);
}

void GenerateChunks(int count)
{
    const int center_x = (int(std::floor(player_pos.x)) >> 4);
    const int center_z = (int(std::floor(player_pos.z)) >> 4);
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
}

void RemoveRedundantChunks()
{
    std::deque<chunk_t *> &chunks = get_chunks();
    chunks.erase(
        std::remove_if(chunks.begin(), chunks.end(),
                       [](chunk_t *&c)
                       {if(!c) return true; if(c->generation_stage == ChunkGenStage::invalid) {delete c; c = nullptr; return true;} return false; }),
        chunks.end());
}

void PrepareChunkRemoval(chunk_t *chunk)
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

void UpdateCamera(camera_t &camera)
{
    player_pos = player->get_position(std::fmod(partialTicks, 1)) - vec3f(0.5, 0.5, 0.5);

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
        target_view_bob_offset = vec3f(0, 0, 0);
        target_view_bob_screen_offset = vec3f(0, 0, 0);
    }
    view_bob_offset = vec3f::lerp(view_bob_offset, target_view_bob_offset, 0.035);
    view_bob_screen_offset = vec3f::lerp(view_bob_screen_offset, target_view_bob_screen_offset, 0.035);
    player_pos = view_bob_offset + player_pos;
    camera.position = player_pos;
    camera.rot.x = xrot;
    camera.rot.y = yrot;
    player->rotation.x = xrot;
    player->rotation.y = yrot;
}

void EditBlocks()
{
    guVector forward = angles_to_vector(xrot, yrot);

    if (!selected_item)
        return;

    if (place_block && (selected_item->empty() || !selected_item->as_item().is_block()))
    {
        place_block = false;
        destroy_block = false;
        return;
    }
    block_t selected_block = block_t{uint8_t(selected_item->id & 0xFF), 0x7F, uint8_t(selected_item->meta & 0xFF)};

    draw_block_outline = raycast_precise(vec3f(player_pos.x + .5, player_pos.y + .5, player_pos.z + .5), vec3f(forward.x, forward.y, forward.z), 4, &raycast_pos, &raycast_face, block_bounds);
    if (draw_block_outline)
    {
        BlockID new_blockid = destroy_block ? BlockID::air : selected_block.get_blockid();
        if (destroy_block || place_block)
        {
            block_t *targeted_block = get_block_at(raycast_pos);
            vec3i editable_pos = destroy_block ? (raycast_pos) : (raycast_pos + raycast_face);
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

                        uint8_t new_meta = raycast_face.y == -1 ? 8 : 0;
                        new_meta ^= same_as_target;

                        if (raycast_face.y != 0 && (new_meta ^ 8) == (targeted_block->meta & 8) && same_as_target)
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
                            if (raycast_face.y == 0 && properties(targeted_blockid).m_render_type == RenderType::slab)
                                editable_block->meta = targeted_block->meta;
                            else if (raycast_face.y == 0)
                                editable_block->meta = new_meta;
                            else
                                editable_block->meta = new_meta ^ same_as_target;
                        }
                    }
                    else
                    {
                        place_block &= old_blockid == BlockID::air || properties(old_blockid).m_fluid;
                        if (!destroy_block && place_block)
                        {
                            editable_block->meta = new_blockid == BlockID::air ? 0 : selected_block.meta;
                            editable_block->set_blockid(new_blockid);
                        }
                    }
                    if (destroy_block)
                    {
                        editable_block->meta = new_blockid == BlockID::air ? 0 : selected_block.meta;
                        editable_block->set_blockid(new_blockid);
                    }
                }

                if (destroy_block)
                {
                    if (!is_remote())
                        DestroyBlock(editable_pos, old_block);
                }
                else if (place_block)
                {
                    if (!is_remote())
                    {
                        update_block_at(editable_pos);
                        update_neighbors(editable_pos);
                    }
                    for (uint8_t face_num = 0; face_num < 6; face_num++)
                    {
                        if (raycast_face == face_offsets[face_num])
                        {
                            TryPlaceBlock(editable_pos, raycast_pos, selected_block, face_num);
                            break;
                        }
                    }
                }
            }
        }
    }

    // Clear the place/destroy block flags to prevent placing blocks immediately.
    place_block = false;
    destroy_block = false;
}

void PlaySound(sound_t sound)
{
    sound_system->play_sound(sound);
}

void AddParticle(const particle_t &particle)
{
    particle_system.add_particle(particle);
}

void SaveWorld()
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
        level_data->setTag("Player", player->serialize());
        level_data->setTag("Time", new NBTTagLong(timeOfDay));
        level_data->setTag("SpawnX", new NBTTagInt(0));
        level_data->setTag("SpawnY", new NBTTagInt(skycast(vec3i(0, 0, 0), nullptr)));
        level_data->setTag("SpawnZ", new NBTTagInt(0));
        level_data->setTag("LastPlayed", new NBTTagLong(time(nullptr) * 1000LL));
        level_data->setTag("LevelName", new NBTTagString("Wii World"));
        level_data->setTag("RandomSeed", new NBTTagLong(world_seed));
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

bool LoadWorld()
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
    timeOfDay = level_data->getLong("Time") % 24000;
    world_seed = level_data->getLong("RandomSeed");
    srand(world_seed);

    // Load the player data if it exists
    NBTTagCompound *player_tag = level_data->getCompound("Player");
    if (player_tag)
        player->deserialize(player_tag);

    // Clean up
    delete level;

    // Based on the player position, the chunks will be loaded around the player - see chunk_new.cpp
    return true;
}

void ResetWorld()
{
    has_loaded = false;
    timeOfDay = 0;
    set_world_hell(false);
    set_world_remote(false);
    lock_t chunk_lock(chunk_mutex);
    if (player)
        player->chunk = nullptr;
    player_inventory.clear();
    for (chunk_t *chunk : get_chunks())
    {
        if (chunk)
            PrepareChunkRemoval(chunk);
    }
    RemoveRedundantChunks();
    mcr::cleanup();
    // Assume the world just started generating
    dirtscreen_text = "Building terrain...";
}

void UpdateNetwork()
{
    if (client.status != Crapper::ErrorStatus::OK)
        return;
    // Receive packets
    client.receive(receive_buffer);

    // Handle packets
    for (int i = 0; i < 10 && client.status == Crapper::ErrorStatus::OK; i++)
    {
        if (client.handlePacket(receive_buffer))
            break;
    }

    // Check for errors
    // NOTE: Won't run if the client is disconnected before the handshake.
    if (client.status != Crapper::ErrorStatus::OK)
    {
        // Reset the world if the connection is lost
        ResetWorld();
        return;
    }
    if (!client.login_success)
        return;

    // Send keep alive every 2700 frames (every 45 seconds)
    if (frameCounter % 2700 == 0)
        client.sendKeepAlive();

    // Send the player's position and look every 12 frames (5 times per second)
    if (frameCounter % 9 == 0 && frameCounter != 0)
        client.sendPlayerPositionLook();

    // Send the player's grounded status if it has changed
    if (frameCounter > 0 && client.on_ground != player->on_ground)
        client.sendGrounded(player->on_ground);
}

void DestroyBlock(const vec3i &pos, block_t old_block)
{
    BlockID old_blockid = old_block.get_blockid();
    set_block_at(pos, BlockID::air);
    update_block_at(pos);

    // Add block particles
    javaport::Random rng;

    int texture_index = get_face_texture_index(&old_block, FACE_NX);

    particle_t particle;
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

        particle_system.add_particle(particle);
    }

    sound_t sound = get_break_sound(old_blockid);
    sound.volume = 0.4f;
    sound.pitch *= 0.8f;
    sound.position = vec3f(pos.x, pos.y, pos.z);
    sound_system->play_sound(sound);

    if (is_remote())
        return;

    // Client side block destruction - only for local play
    update_neighbors(pos);
    properties(old_blockid).m_destroy(pos, old_block);
    SpawnDrop(pos, old_block, properties(old_blockid).m_drops(old_block));
}

void TryPlaceBlock(const vec3i &pos, const vec3i &targeted, block_t new_block, uint8_t face)
{
    sound_t sound = get_mine_sound(new_block.get_blockid());
    sound.volume = 0.4f;
    sound.pitch *= 0.8f;
    sound.position = vec3f(pos.x, pos.y, pos.z);
    sound_system->play_sound(sound);
    player_inventory[selected_hotbar_slot].count--;
    if (player_inventory[selected_hotbar_slot].count == 0)
        player_inventory[selected_hotbar_slot] = inventory::item_stack();

    if (is_remote())
        client.sendPlaceBlock(targeted.x, targeted.y, targeted.z, (face + 4) % 6, new_block.id, 1, new_block.meta);
}

void SpawnDrop(const vec3i &pos, const block_t &old_block, inventory::item_stack item)
{
    if (item.empty())
        return;
    chunk_t *chunk = get_chunk_from_pos(pos);
    if (!chunk)
        return;
    // Drop items
    javaport::Random rng;
    vec3f item_pos = vec3f(pos.x, pos.y, pos.z) + vec3f(0.5);
    item_entity_t *entity = new item_entity_t(item_pos, item);
    entity->ticks_existed = 10; // Halves the pickup delay (20 ticks / 2 = 10)
    entity->velocity = vec3f(rng.nextFloat() - .5f, rng.nextFloat(), rng.nextFloat() - .5f) * 0.25f;
    chunk->entities.push_back(entity);
}

void CreateExplosion(vec3f pos, float power, chunk_t *near)
{
    explode(pos, power * 0.75f, near);

    javaport::Random rng;

    sound_t sound = get_sound("old_explode");
    sound.position = pos;
    sound.volume = 0.5;
    sound.pitch = 0.8;
    sound_system->play_sound(sound);

    particle_t particle;
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

        particle_system.add_particle(particle);
    }
}

void UpdateChunkData(frustum_t &frustum, std::deque<chunk_t *> &chunks)
{
    int light_up_calls = 0;
    for (chunk_t *&chunk : chunks)
    {
        if (chunk)
        {
            float hdistance = chunk->player_taxicab_distance();
            if (hdistance > RENDER_DISTANCE * 24 + 24 && !is_remote())
            {
                PrepareChunkRemoval(chunk);
                continue;
            }

            if (chunk->generation_stage != ChunkGenStage::done)
                continue;

            bool visible = (hdistance <= std::max(RENDER_DISTANCE * 16 * fog_depth_multiplier, 16.0f));

            // Tick chunks
            for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
            {
                chunkvbo_t &vbo = chunk->vbos[j];
                vbo.x = chunk->x * 16;
                vbo.y = j * 16;
                vbo.z = chunk->z * 16;
                float vdistance = std::abs(vbo.y - player_pos.y);
                vbo.visible = visible && (vdistance <= std::max(RENDER_DISTANCE * 12 * fog_depth_multiplier, 16.0f));
                if (!is_remote() && chunk->has_fluid_updates[j] && vdistance <= SIMULATION_DISTANCE * 16 && tickCounter - lastWaterTick >= 5)
                    RecalcSectionWater(chunk, j);
            }
            if (!chunk->lit_state && light_up_calls < 5)
            {
                light_up_calls++;
                chunk->light_up();
            }
        }
    }
    if (tickCounter - lastWaterTick >= 5)
    {
        lastWaterTick = tickCounter;
        fluidUpdateCount++;
    }
}

bool SortVBOs(chunkvbo_t *&a, chunkvbo_t *&b)
{
    return *a < *b;
}

void UpdateChunkVBOs(std::deque<chunk_t *> &chunks)
{
    static std::vector<chunkvbo_t *> vbos_to_update;
    std::vector<chunkvbo_t *> vbos_to_rebuild;
    if (!light_engine_busy())
    {
        for (chunk_t *&chunk : chunks)
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
        std::sort(vbos_to_rebuild.begin(), vbos_to_rebuild.end(), SortVBOs);
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
        }
        vbos_to_update.clear();
    }
}

void UpdatePlayer()
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
                aabb_entity_t *&entity = e.second;

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
    vec3i block_pos = vec3i(std::floor(player->position.x), std::floor(player->aabb.min.y + player->y_offset), std::floor(player->position.z));
    block_t *block = get_block_at(block_pos);
    if (block && properties(block->id).m_fluid && block_pos.y + 2 - get_fluid_height(block_pos, block->get_blockid(), player->chunk) >= player->aabb.min.y + player->y_offset)
    {
        in_fluid = properties(block->id).m_base_fluid;
    }
    else
    {
        in_fluid = BlockID::air;
    }
}

void UpdateScene(frustum_t &frustum)
{
    std::deque<chunk_t *> &chunks = get_chunks();
    if (!is_remote() && !light_engine_busy())
    {
        GenerateChunks(1);
    }
    RemoveRedundantChunks();
    EditBlocks();
    UpdateChunkData(frustum, chunks);
    UpdateChunkVBOs(chunks);

    // Update the particle system
    particle_system.update(0.025);

    // Calculate chunk memory usage
    total_chunks_size = 0;
    for (chunk_t *&chunk : chunks)
    {
        total_chunks_size += chunk ? chunk->size() : 0;
    }
}

void UpdateInventory(gertex::GXView &viewport)
{
    if (!current_gui)
        return;

    cursor_x += left_stick.x * 8;
    cursor_y -= left_stick.y * 8;

    if (!wiimote_ir_visible)
    {
        // Since you can technically go out of bounds using a joystick, we need to clamp the cursor position; it'd be really annoying losing the cursor
        cursor_x = std::clamp(cursor_x, 0, int(viewport.width));
        cursor_y = std::clamp(cursor_y, 0, int(viewport.height));
    }
    current_gui->update();
}

void DrawHUD(gertex::GXView &viewport)
{
    // Disable depth testing for GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);

    // Reset the orthogonal position matrix and push it onto the stack
    gertex::ortho(viewport);

    // Draw crosshair
    int crosshair_x = int(viewport.width - 32) >> 1;
    int crosshair_y = int(viewport.height - 32) >> 1;

    draw_textured_quad(icons_texture, crosshair_x, crosshair_y, 32, 32, 0, 0, 16, 16);

    // Draw IR cursor if visible
    if (wiimote_ir_visible)
    {
        draw_textured_quad(icons_texture, wiimote_ir_x * viewport.width - 7, wiimote_ir_y * viewport.height - 3, 48, 48, 32, 32, 56, 56);
    }

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw the hotbar background
    draw_textured_quad(icons_texture, (viewport.width - 364) / 2, viewport.height - 44, 364, 44, 56, 9, 238, 31);

    // Draw the hotbar selection
    draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + selected_hotbar_slot * 40 - 2, viewport.height - 46, 48, 48, 56, 31, 80, 55);

    // Push the orthogonal position matrix onto the stack
    gertex::push_matrix();

    // Draw the hotbar items
    for (size_t i = 0; i < 9; i++)
    {
        gui::draw_item((viewport.width - 364) / 2 + i * 40 + 6, viewport.height - 38, player_inventory[i]);
    }

    // Restore the orthogonal position matrix
    gertex::pop_matrix();
    gertex::load_pos_matrix();

    // Enable direct colors as the previous call to draw_item may have changed the color mode
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw the player's health above the hotbar. The texture size is 9x9 but they overlap by 1 pixel.
    int health = player ? int8_t(player->health) : 0;

    // Empty hearts
    for (int i = 0; i < 10; i++)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + i * 16, viewport.height - 64, 18, 18, 16, 0, 25, 9);
    }

    // Full hearts
    for (int i = 0; i < (health & ~1); i += 2)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + i * 8, viewport.height - 64, 18, 18, 52, 0, 61, 9);
    }

    // Half heart (if the player has an odd amount of health)
    if ((health & 1) == 1)
    {
        draw_textured_quad(icons_texture, (viewport.width - 364) / 2 + (health & ~1) * 8, viewport.height - 64, 18, 18, 61, 0, 70, 9);
    }
}

void DrawInventory(gertex::GXView &viewport)
{
    if (!current_gui)
        return;

    // This is required as blocks require a dummy chunk to be rendered
    if (!get_chunks().size())
        return;

    // Disable depth testing for GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);

    // Reset the orthogonal position matrix and push it onto the stack
    gertex::ortho(viewport);
    gertex::push_matrix();

    // Draw the GUI elements
    current_gui->draw();

    // Restore the orthogonal position matrix
    gertex::pop_matrix();
    gertex::load_pos_matrix();

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw the cursor
    if (wiimote_ir_visible && !current_gui->contains(cursor_x, cursor_y))
        draw_textured_quad(icons_texture, cursor_x - 7, cursor_y - 3, 32, 48, 32, 32, 48, 56);
    else
        draw_textured_quad(icons_texture, cursor_x - 16, cursor_y - 16, 32, 32, 0, 32, 32, 64);
}

void DrawSelectedBlock()
{
    selected_item = &player_inventory[selected_hotbar_slot];
    if (get_chunks().size() == 0 || !selected_item || selected_item->empty())
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
    vec3f selectedBlockPos = vec3f(+.625f, -.75f, -.75f) + vec3f(-view_bob_screen_offset.x, view_bob_screen_offset.y, 0);

    int texture_index;
    char *texbuf;

    // Check if the selected item is a block
    if (selected_item->as_item().is_block())
    {
        block_t selected_block = block_t{uint8_t(selected_item->id & 0xFF), 0x7F, uint8_t(selected_item->meta & 0xFF)};
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
        texture_index = get_default_texture_index(BlockID(selected_item->id));

        // Use the blockmap texture
        use_texture(blockmap_texture);
        texbuf = (char *)MEM_PHYSICAL_TO_K0(GX_GetTexObjData(&blockmap_texture));
    }
    else
    {
        // Setup item properties

        // Get the texture index of the selected item
        texture_index = selected_item->as_item().texture_index;

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

void DrawScene(bool transparency)
{
    std::deque<chunk_t *> &chunks = get_chunks();
    // Use terrain texture
    use_texture(blockmap_texture);

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Draw the solid pass
    if (!transparency)
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
                    if (vbo.cached_solid)
                    {
                        Render(chunkPos, vbo.cached_solid.buffer, vbo.cached_solid.length);
                    }
                }
                chunk->render_entities(partialTicks, false);
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
                    if (vbo.cached_transparent)
                    {
                        Render(chunkPos, vbo.cached_transparent.buffer, vbo.cached_transparent.length);
                    }
                }
                chunk->render_entities(partialTicks, true);
            }
        }
    }

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw block outlines
    if (!transparency && draw_block_outline)
    {
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        vec3f outline_pos = raycast_pos - vec3f(0.5, 0.5, 0.5);

        vec3f towards_camera = vec3f(player_pos) - outline_pos;
        towards_camera.normalize();
        towards_camera = towards_camera * 0.002;

        vec3f b_min = vec3f(raycast_pos.x, raycast_pos.y, raycast_pos.z) + vec3f(1.0, 1.0, 1.0);
        vec3f b_max = vec3f(raycast_pos.x, raycast_pos.y, raycast_pos.z);
        for (aabb_t &bounds : block_bounds)
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
        DrawOutline(block_outer_bounds);
    }
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
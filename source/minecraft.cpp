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
#include "chunk_new.hpp"
#include "block.hpp"
#include "blocks.hpp"
#include "brightness_values.h"
#include "vec3i.hpp"
#include "vec3d.hpp"
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

f32 xrot = 0.0f;
f32 yrot = 0.0f;
aabb_entity_t *player = nullptr;
guVector player_pos = {0.F, 80.0F, 0.F};
void *frameBuffer[2] = {NULL, NULL};
static GXRModeObj *rmode = NULL;

bool draw_block_outline = false;
vec3i raycast_pos;
vec3i raycast_face;
std::vector<aabb_t> block_bounds;

int dropFrames = 0;

int frameCounter = 0;
int tickCounter = 0;
int lastEntityTick = 0;
int lastWaterTick = 0;
int fluidUpdateCount = 0;
float lastStepDistance = 0;
float deltaTime = 0.0f;
float partialTicks = 0.0f;

uint32_t total_chunks_size = 0;

u32 wiimote_down = 0;
u32 wiimote_held = 0;
float wiimote_x = 0;
float wiimote_z = 0;
float wiimote_rx = 0;
float wiimote_ry = 0;
int shoulder_btn_frame_counter = 0;
float prev_left_shoulder = 0;
float prev_right_shoulder = 0;
bool destroy_block = false;
bool place_block = false;
block_t selected_block = {uint8_t(BlockID::stone), 0x7F, 0, 0xF, 0xF};

vec3f view_bob_offset(0, 0, 0);
vec3f view_bob_screen_offset(0, 0, 0);

particle_system_t particle_system;
sound_system_t *sound_system = nullptr;

bool inventory_visible = false;
bool show_dirtscreen = true;
bool has_loaded = false;

float fog_depth_multiplier = 1.0f;

void CreateExplosion(vec3f pos, float power, chunk_t *near);
void UpdateLoadingStatus();
void UpdateLightDir();
void DrawInventory(view_t &view);
void DrawSelectedBlock();
void DrawScene(std::deque<chunk_t *> &chunks, bool transparency);
void GenerateChunks(int count);
void RemoveRedundantChunks(std::deque<chunk_t *> &chunks);
void PrepareChunkRemoval(chunk_t *chunk);
void UpdateChunkData(frustum_t &frustum, std::deque<chunk_t *> &chunks);
void UpdateScene(frustum_t &frustum, std::deque<chunk_t *> &chunks);
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

    CON_Init(frameBuffer[0], 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    // Configure video
    VIDEO_Configure(rmode);
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
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
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
    view_t viewport = view_t(rmode->fbWidth, rmode->efbHeight, CONF_GetAspectRatio(), 90, CAMERA_NEAR, CAMERA_FAR, yscale);

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
    chdir("/apps/minecraft/resources/");

    printf("Render resolution: %f,%f, Widescreen: %s\n", viewport.width, viewport.height, viewport.widescreen ? "Yes" : "No");
    light_engine_init();
    srand(gettime());
    init_chunks();
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

    bool in_fluid = false;
    bool in_lava = false;

    while (!isExiting)
    {
        u64 frame_start = time_get();
        float sky_multiplier = get_sky_multiplier();
        if (HWButton != -1)
            break;
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
#ifdef MONO_LIGHTING
        LightMapBlend(light_day_mono_rgba, light_night_mono_rgba, light_map, 255 - uint8_t(sky_multiplier * 255));
#else
        LightMapBlend(light_day_rgba, light_night_rgba, light_map, 255 - uint8_t(sky_multiplier * 255));
#endif
        GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));
        GX_InvVtxCache();
        background = get_sky_color();
        GX_SetCopyClear(background, 0x00FFFFFF);

        fog_depth_multiplier = flerp(fog_depth_multiplier, std::min(std::max(player_pos.y, 24.f) / 36.f, 1.0f), 0.05f);

        if (player_pos.y < -999)
        {
            int y = skycast(vec3i(0, 0, 0)) + 3;

            if (y > -999)
            {
                // Check if the player entity exists
                if (player)
                {
                    // Update the player entity's position
                    player->set_position(player_pos);
                }
                else
                {
                    // Add the player to the chunks
                    chunk_t *player_chunk = get_chunk_from_pos(vec3i(0, 0, 0), false, false);
                    if (player_chunk)
                    {
                        player = new aabb_entity_t(0.6, 1.8);
                        player->local = true;
                        player->y_offset = 1.62;
                        player->y_size = 0;
                        player->set_position(vec3f(0.5, y, 0.5));
                        player_chunk->entities.push_back(player);
                    }
                }
            }
        }
        // Enable fog
        if (in_fluid)
        {
            background = in_lava ? GXColor{0xFF, 0, 0, 0xFF} : GXColor{0, 0, 0xFF, 0xFF};
            GX_SetCopyClear(background, 0x00FFFFFF);
            float fog_multiplier = in_lava ? 0.05f : 0.6f;
            use_fog(true, viewport, background, fog_depth_multiplier * fog_multiplier * (GENERATION_DISTANCE * 0.5f - 8), fog_depth_multiplier * fog_multiplier * (GENERATION_DISTANCE * 0.5f));
        }
        else
            use_fog(true, viewport, background, fog_depth_multiplier * (GENERATION_DISTANCE * 0.5f - 8), fog_depth_multiplier * (GENERATION_DISTANCE * 0.5f));

        UpdateLightDir();

        if (frameCounter % 3 == 0)
        {
            update_textures();
        }

        GetInput();

        for (int i = lastEntityTick, count = 0; i < tickCounter && count < 10; i++, count++)
        {
            for (chunk_t *&chunk : chunks)
            {
                // Update the entities in the chunk
                chunk->update_entities();
            }
            wiimote_down = 0;
            wiimote_held = 0;
        }
        lastEntityTick = tickCounter;

        if (player)
        {
            // Update the sound system
            sound_system->update(angles_to_vector(0, yrot + 90), player->get_position(std::fmod(partialTicks, 1)));
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
        }

        // Construct the view frustum matrix from the camera
        frustum_t frustum = calculate_frustum(camera);

        UpdateScene(frustum, chunks);

        use_perspective(viewport);

        UpdateLoadingStatus();

        if (!show_dirtscreen)
        {

            // Enable backface culling for terrain
            GX_SetCullMode(GX_CULL_BACK);

            // Prepare the transformation matrix
            transform_view(get_view_matrix(), guVector{0, 0, 0});

            // Prepare opaque rendering parameters
            GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
            GX_SetAlphaUpdate(GX_TRUE);

            // Draw particles
            GX_SetAlphaCompare(GX_GEQUAL, 1, GX_AOP_AND, GX_ALWAYS, 0);
            draw_particles(camera, particle_system.particles, particle_system.size());

            // Draw chunks
            GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
            DrawScene(chunks, false);

            // Prepare transparent rendering parameters
            GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
            GX_SetAlphaUpdate(GX_FALSE);

            // Draw chunks
            GX_SetAlphaCompare(GX_GEQUAL, 1, GX_AOP_AND, GX_ALWAYS, 0);
            DrawScene(chunks, true);

            // Draw selected block
            DrawSelectedBlock();

            // Draw sky
            if (!in_fluid)
                draw_sky(background);
        }

        use_ortho(viewport);
        // Use 0 fractional bits for the position data, because we're drawing in pixel space.
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);

        // Disable fog
        use_fog(false, viewport, background, 0, 0);

        // Enable direct colors
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

        // Draw GUI elements
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
        GX_SetAlphaUpdate(GX_TRUE);

        static vec3f pan_underwater_texture(0, 0, 0);
        pan_underwater_texture = pan_underwater_texture + vec3f(wiimote_rx * 0.25, wiimote_ry * 0.25, 0.0);
        pan_underwater_texture.x = std::fmod(pan_underwater_texture.x, viewport.width);
        pan_underwater_texture.y = std::fmod(pan_underwater_texture.y, viewport.height);
        if (in_fluid && !in_lava)
        {
            draw_textured_quad(underwater_texture, pan_underwater_texture.x - viewport.width, pan_underwater_texture.y - viewport.height, viewport.width * 3, viewport.height * 3, 0, 0, 48, 48);
            draw_textured_quad(vignette_texture, 0, 0, viewport.width, viewport.height, 0, 0, 256, 256);
        }
        draw_textured_quad(vignette_texture, 0, 0, viewport.width, viewport.height, 0, 0, 256, 256);
        if (show_dirtscreen)
        {
            int texture_index = get_default_texture_index(BlockID::dirt);
            fill_screen_texture(blockmap_texture, viewport, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));
        }
        else if (!inventory_visible)
        {
            // Draw crosshair
            int crosshair_x = int(viewport.width - 32) >> 1;
            int crosshair_y = int(viewport.height - 32) >> 1;

            draw_textured_quad(icons_texture, crosshair_x, crosshair_y, 32, 32, 0, 0, 16, 16);
        }
        else
            DrawInventory(viewport);

        if (player)
        {
            vec3i block_pos = vec3i(std::floor(player->position.x), std::floor(player->aabb.min.y + player->y_offset), std::floor(player->position.z));
            block_t *block = get_block_at(block_pos);
            in_fluid = false;
            if (block && properties(block->id).m_fluid && block_pos.y + 2 - get_fluid_height(block_pos, block->get_blockid(), player->chunk) >= player->aabb.min.y + player->y_offset)
            {
                in_fluid = true;
                in_lava = properties(block->id).m_base_fluid == BlockID::lava;
            }
        }
        GX_DrawDone();

        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        frameCounter++;
        deltaTime = time_diff_s(frame_start, time_get());

        // Ensure that the delta time is not too large to prevent issues
        if (deltaTime > 0.05f)
            deltaTime = 0.05f;

        partialTicks += deltaTime * 20.0f;
        tickCounter += int(partialTicks);
        partialTicks -= int(partialTicks);
        fb ^= 1;
    }
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
        SYS_ResetSystem(HWButton, 0, 0);
    }
    return 0;
}

void UpdateLoadingStatus()
{
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
                chunk_t *chunk = get_chunk_from_pos(chunk_pos, false, false);
                if (!chunk)
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
    u32 raw_wiimote_down = WPAD_ButtonsDown(0);
    u32 raw_wiimote_held = WPAD_ButtonsHeld(0);
    if ((raw_wiimote_down & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME)))
        isExiting = true;
    if ((raw_wiimote_down & WPAD_BUTTON_1))
    {
        printf("PRIM_CHUNK_MEMORY: %d B\n", total_chunks_size);
        printf("TICK: %d, WATER: %d\n", tickCounter, lastWaterTick);
        printf("SPEED: %f, %f\n", wiimote_x, wiimote_z);
    }
    if ((raw_wiimote_down & WPAD_BUTTON_2))
    {
        tickCounter += 6000;
    }

    if (player && (raw_wiimote_down & WPAD_CLASSIC_BUTTON_DOWN) != 0)
    {
        // Spawn a creeper at the player's position
        vec3f pos = player->get_position(std::fmod(partialTicks, 1));
        chunk_t *creeper_chunk = get_chunk_from_pos(vec3i(pos.x, pos.y, pos.z), false, false);
        if (creeper_chunk)
        {
            creeper_entity_t *creeper = new creeper_entity_t(pos);
            creeper->chunk = creeper_chunk;
            creeper_chunk->entities.push_back(creeper);
        }
    }
    expansion_t expansion;
    WPAD_Expansion(0, &expansion);
    vec3f left_stick(0, 0, 0);
    vec3f right_stick(0, 0, 0);
    float sensitivity = LOOKAROUND_SENSITIVITY;
    if (expansion.type == WPAD_EXP_NONE || expansion.type == WPAD_EXP_UNKNOWN)
    {
        return;
    }
    else if (expansion.type == WPAD_EXP_NUNCHUK)
    {
        // Get the left stick position from the D-pad
        left_stick = vec3f(0, 0, 0);

        u32 nunchuk_held = expansion.nunchuk.btns_held;
        u32 nunchuk_down = expansion.nunchuk.btns_held & ~prev_nunchuk_held;
        prev_nunchuk_held = nunchuk_held;

        u32 new_raw_wiimote_down = 0;
        u32 new_raw_wiimote_held = 0;

        if ((raw_wiimote_held & WPAD_BUTTON_LEFT))
            left_stick.x--;
        if ((raw_wiimote_held & WPAD_BUTTON_RIGHT))
            left_stick.x++;
        if ((raw_wiimote_held & WPAD_BUTTON_UP))
            left_stick.y++;
        if ((raw_wiimote_held & WPAD_BUTTON_DOWN))
            left_stick.y--;

        if (left_stick.magnitude() > 0.001)
            left_stick = left_stick.normalize();

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
        right_stick.x = float(int(expansion.nunchuk.js.pos.x) - int(expansion.nunchuk.js.center.x)) * 2 / (int(expansion.nunchuk.js.max.x) - 2 - int(expansion.nunchuk.js.min.x));
        right_stick.y = float(int(expansion.nunchuk.js.pos.y) - int(expansion.nunchuk.js.center.y)) * 2 / (int(expansion.nunchuk.js.max.y) - 2 - int(expansion.nunchuk.js.min.y));
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

    inventory_visible ^= (raw_wiimote_down & WPAD_CLASSIC_BUTTON_X) != 0;

    if (!inventory_visible)
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
        do
        {
            // Decrease the selected block id by 1 unless we're at the lowest block id already
            if (selected_block.id > 0)
            {
                selected_block.id--;
            }
            else
            {
                break; // Break out of the loop if we're at the lowest block id
            }
            // If the selected block is air, don't allow the player to select it
        } while (!properties(selected_block.id).m_valid_item);
        selected_block.meta = properties(selected_block.id).m_default_state;
    }
    if (raw_wiimote_down & WPAD_CLASSIC_BUTTON_ZR)
    {
        do
        {
            // Increase the selected block id by 1 unless we're at the highest block id already
            if (selected_block.id < 255)
            {
                selected_block.id++;
            }
            else
            {
                break; // Break out of the loop if we're at the highest block id
            }
            // If the selected block is air, don't allow the player to select it
        } while (!properties(selected_block.id).m_valid_item);
        selected_block.meta = properties(selected_block.id).m_default_state;
    }
    if (selected_block.get_blockid() == BlockID::air)
        selected_block.set_blockid(BlockID::stone);

    if ((raw_wiimote_down & WPAD_CLASSIC_BUTTON_LEFT))
    {
        if (player)
        {
            vec3i block_pos;
            vec3i face;
            if (raycast(vec3f(player_pos.x + .5, player_pos.y + .5, player_pos.z + .5), angles_to_vector(xrot, yrot), 128, &block_pos, &face))
            {
                block_pos = block_pos + face;
                vec3f pos = vec3f(block_pos.x, block_pos.y, block_pos.z) + vec3f(0.5, 0.5, 0.5);
                CreateExplosion(pos, 3, get_chunk_from_pos(block_pos, false, false));
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

void Render(guVector chunkPos, void *buffer, u32 length)
{
    if (!buffer || !length)
        return;
    transform_view(get_view_matrix(), chunkPos);
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
    const int start_x = (int(player_pos.x - GENERATION_DISTANCE) & ~15);
    const int start_z = (int(player_pos.z - GENERATION_DISTANCE) & ~15);
    const int end_x = start_x + GENERATION_DISTANCE * 2;
    const int end_z = start_z + GENERATION_DISTANCE * 2;

    int generated = 0;
    for (int x = start_x; count && x <= end_x; x += 16)
    {
        for (int z = start_z; count && z <= end_z; z += 16)
        {
            if (!get_chunk_from_pos(vec3i(x, 0, z), true, false))
            {
                count--;
                generated++;
            }
        }
    }
}

void RemoveRedundantChunks(std::deque<chunk_t *> &chunks)
{
    WRAP_ASYNC_FUNC(chunk_mutex, chunks.erase(
                                     std::remove_if(chunks.begin(), chunks.end(),
                                                    [](chunk_t *&c)
                                                    {if(!c) return true; if(!c->valid) {delete c; c = nullptr; return true;} return false; }),
                                     chunks.end()));
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
    chunk->valid = false;
}

void EditBlocks()
{
    guVector forward = angles_to_vector(xrot, yrot);

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
                // Handle slab placement
                if (properties(new_blockid).m_render_type == RenderType::slab)
                {
                    bool same_as_target = targeted_block->get_blockid() == new_blockid;

                    uint8_t new_meta = raycast_face.y == -1 ? 1 : 0;
                    new_meta ^= same_as_target;

                    if (raycast_face.y != 0 && (new_meta ^ 1) == (targeted_block->meta & 1) && same_as_target)
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

                if (place_block || destroy_block)
                    update_block_at(editable_pos);
                update_neighbors(editable_pos);

                if (destroy_block)
                {
                    // Add block particles

                    int texture_index = get_default_texture_index(old_blockid);

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
                        particle.position = vec3f(editable_pos.x, editable_pos.y, editable_pos.z) + vec3f(JavaLCGFloat() - .5f, JavaLCGFloat() - .5f, JavaLCGFloat() - .5f);
                        particle.velocity = vec3f(JavaLCGFloat() - .5f, JavaLCGFloat() - .25f, JavaLCGFloat() - .5f) * 7.5;

                        // Randomize the particle texture coordinates
                        particle.u = u + ((rand() & 3) << 2);
                        particle.v = v + ((rand() & 3) << 2);

                        // Randomize the particle life time by up to 10 ticks
                        particle.life_time = particle.max_life_time - (rand() % 10);

                        particle_system.add_particle(particle);
                    }

                    sound_t sound = get_break_sound(old_blockid);
                    sound.volume = 0.4f;
                    sound.pitch *= 0.8f;
                    sound.position = vec3f(editable_pos.x, editable_pos.y, editable_pos.z);
                    sound_system->play_sound(sound);

                    if (old_blockid == BlockID::tnt)
                    {
                        get_chunk_from_pos(editable_pos, false)->entities.push_back(new exploding_block_entity_t(old_block, editable_pos, 80));
                    }
                }
                else if (place_block)
                {
                    sound_t sound = get_mine_sound(new_blockid);
                    sound.volume = 0.4f;
                    sound.pitch *= 0.8f;
                    sound.position = vec3f(editable_pos.x, editable_pos.y, editable_pos.z);
                    sound_system->play_sound(sound);
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

void CreateExplosion(vec3f pos, float power, chunk_t *near)
{
    explode(pos, power * 0.75f, near);

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
        particle.position = pos + vec3f(JavaLCGFloat() - .5f, JavaLCGFloat() - .5f, JavaLCGFloat() - .5f) * power * 2;

        // Randomize the particle life time by up to 10 ticks
        particle.life_time = particle.max_life_time - (rand() % 20) - 20;

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
        if (chunk && chunk->valid)
        {
            float hdistance = std::max(std::abs((chunk->x * 16 + 8) - player_pos.x), std::abs((chunk->z * 16 + 8) - player_pos.z));
            if (hdistance > RENDER_DISTANCE * 16)
            {
                PrepareChunkRemoval(chunk);
                continue;
            }

            // Tick chunks
            for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
            {
                chunkvbo_t &vbo = chunk->vbos[j];
                vbo.x = chunk->x * 16;
                vbo.y = j * 16;
                vbo.z = chunk->z * 16;
                float vdistance = std::abs(vbo.y - player_pos.y);
                vbo.visible = (vdistance <= std::max(RENDER_DISTANCE * 12 * fog_depth_multiplier, 16.0f)) && (hdistance <= std::max(RENDER_DISTANCE * 16 * fog_depth_multiplier, 16.0f));
                if (chunk->has_fluid_updates[j] && vdistance <= SIMULATION_DISTANCE * 16 && tickCounter - lastWaterTick >= 5)
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
            if (chunk && chunk->valid && !chunk->light_update_count)
            {
                // Check if chunk has other chunks around it.
                bool surrounding = true;
                for (int i = 0; i < 6; i++)
                {
                    // Skip the top and bottom faces
                    if (i == 2)
                        i = 4;
                    // Check if the surrounding chunk exists and has no lighting updates pending
                    chunk_t *surrounding_chunk = get_chunk(vec3i(chunk->x + face_offsets[i].x, 0, chunk->z + face_offsets[i].z), false);
                    if (!surrounding_chunk || surrounding_chunk->light_update_count)
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
            chunk_t *chunk = get_chunk_from_pos(vec3i(vbo.x, 0, vbo.z), false);
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

void UpdateScene(frustum_t &frustum, std::deque<chunk_t *> &chunks)
{
    if (!light_engine_busy())
        GenerateChunks(1);
    RemoveRedundantChunks(chunks);
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

void DrawInventory(view_t &viewport)
{
    std::deque<chunk_t *> &chunks = get_chunks();
    if (chunks.size() == 0)
        return;

    // Disable depth testing for GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);

    // Prepare position matrix for rendering GUI elements
    Mtx tmp_matrix;
    guMtxIdentity(tmp_matrix);
    Mtx inventory_matrix;
    guMtxIdentity(inventory_matrix);
    Mtx inventory_flat_matrix;
    guMtxIdentity(inventory_flat_matrix);

    guMtxRotDeg(tmp_matrix, 'x', 202.5F);
    guMtxConcat(inventory_matrix, tmp_matrix, inventory_matrix);

    guMtxRotDeg(tmp_matrix, 'y', 45.0F);
    guMtxConcat(inventory_matrix, tmp_matrix, inventory_matrix);

    guMtxScaleApply(inventory_matrix, inventory_matrix, 0.65, 0.65, 0.65f);
    guMtxTransApply(inventory_matrix, inventory_matrix, viewport.width * 0.5f, viewport.height * 0.5f, -10.0f);

    guMtxScaleApply(inventory_flat_matrix, inventory_flat_matrix, 1.0f, 1.0f, 1.0f);
    guMtxTransApply(inventory_flat_matrix, inventory_flat_matrix, viewport.width * 0.5f, viewport.height * 0.5f, -10.0f);

    GX_LoadPosMtxImm(inventory_flat_matrix, GX_PNMTX0);

    // Disable backface culling for the inventory background
    GX_SetCullMode(GX_CULL_NONE);

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw the inventory background
    draw_simple_textured_quad(container_texture, -176, -220, 512, 512);

    // Enable backface culling for blocks
    GX_SetCullMode(GX_CULL_BACK);

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Draw the blocks in the inventory
    use_texture(blockmap_texture);
    block_t template_block = {0x00, 0x7F, 0, 0xF, 0xF};
    for (int i = 0; i < 54; i++)
    {
        // Calculate the position of the block in the inventory
        int x = i % 9;
        int y = i / 9;
        int x_offset = 36 * (x - 4);
        int y_offset = 36 * (y - 4) - 24;

        // Set the block id of the template block
        do
        {
            template_block.id++;
        } while (!properties(template_block.id).m_valid_item);

        template_block.meta = block_properties[template_block.id].m_default_state;
        RenderType render_type = properties(template_block.id).m_render_type;
        if (!properties(template_block.id).m_fluid && (render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
        {
            // Translate the block to the correct position
            guMtxCopy(inventory_matrix, tmp_matrix);
            guMtxTransApply(tmp_matrix, tmp_matrix, x_offset, y_offset, 0.0f);
            GX_LoadPosMtxImm(tmp_matrix, GX_PNMTX0);

            // Draw the block
            render_single_block(template_block, false);
            render_single_block(template_block, true);
        }
        else
        {
            guMtxCopy(inventory_flat_matrix, tmp_matrix);
            guMtxTransApply(tmp_matrix, tmp_matrix, x_offset, y_offset, 0.0f);
            GX_LoadPosMtxImm(tmp_matrix, GX_PNMTX0);

            int texture_index = get_default_texture_index(BlockID(template_block.id));
            render_single_item(texture_index, false);
            render_single_item(texture_index, true);
        }
    }
}

void DrawSelectedBlock()
{
    std::deque<chunk_t *> &chunks = get_chunks();
    if (chunks.size() == 0)
        return;
    // Get the block at the player's position
    block_t *view_block = get_block_at(vec3i(std::round(player_pos.x), std::round(player_pos.y), std::round(player_pos.z)));
    if (view_block)
    {
        // Set the light level of the selected block
        selected_block.light = view_block->light;
    }
    else
    {
        selected_block.light = 0xFF;
    }

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Specify the selected block offset
    vec3f selectedBlockPos = vec3f(+.625f, -.625f, -.625f) + vec3f(-view_bob_screen_offset.x, view_bob_screen_offset.y, 0);

    // Transform the selected block position
    transform_view_screen(get_view_matrix(), selectedBlockPos, guVector{.5f, .5f, .5f}, guVector{10, -45, 0});

    // Draw the opaque pass
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
    render_single_block(selected_block, false);

    // Draw the transparent pass
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_TRUE);
    render_single_block(selected_block, true);
}

void DrawScene(std::deque<chunk_t *> &chunks, bool transparency)
{
    // Use terrain texture
    use_texture(blockmap_texture);

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Draw the solid pass
    if (!transparency)
    {
        for (chunk_t *&chunk : chunks)
        {
            if (chunk && chunk->valid && chunk->lit_state)
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
                chunk->render_entities(partialTicks);
            }
        }
    }
    // Draw the transparent pass
    else
    {
        for (chunk_t *&chunk : chunks)
        {
            if (chunk && chunk->valid && chunk->lit_state)
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
                chunk->render_entities(partialTicks);
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

        transform_view(get_view_matrix(), floor_b_min + towards_camera - vec3f(0.5, 0.5, 0.5));

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
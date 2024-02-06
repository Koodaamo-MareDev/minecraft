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
#include "threadhandler.hpp"
#include "chunk_new.hpp"
#include "block.hpp"
#include "blocks.hpp"
#include "light_day_rgba.h"
#include "light_night_rgba.h"
#include "brightness_values.h"
#include "vec3i.hpp"
#include "vec3d.hpp"
#include "timers.hpp"
#include "texturedefs.h"
#include "cpugx.hpp"
#include "raycast.hpp"
#include "light.hpp"
#include "texanim.hpp"
#include "base3d.hpp"
#include "render.hpp"
#include "render_gui.hpp"
#define DEFAULT_FIFO_SIZE (1024 * 1024)
#define CLASSIC_CONTROLLER_THRESHOLD 4
#define MAX_PARTICLES 100
#define LOOKAROUND_SENSITIVITY 360
#define MOVEMENT_SPEED 10
#define DIRECTIONAL_LIGHT GX_ENABLE

bool debug_spritesheet = false;
lwp_t main_thread;
f32 xrot = 0.0f;
f32 yrot = 0.0f;
guVector player_pos = {0.F, 80.0F, 0.F};
void *frameBuffer[2] = {NULL, NULL};
static GXRModeObj *rmode = NULL;

void *outline = nullptr;
u32 outline_len = 0;

bool draw_block_outline = false;
vec3i raycast_block;
vec3i raycast_block_face;

int dropFrames = 0;

int frameCounter = 0;
int tickCounter = 0;
float deltaTime = 0.0f;
float partialTicks = 0.0f;

uint32_t total_chunks_size = 0;

int shoulder_btn_frame_counter = 0;
float prev_left_shoulder = 0;
float prev_right_shoulder = 0;
bool destroy_block = false;
bool place_block = false;

void UpdateLightDir();
void DrawScene(Mtx view, frustum_t &frustum, std::deque<chunk_t *> &chunks, bool transparency);
void UpdateScene(std::deque<chunk_t *> &chunks);
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
    // LWP_ResumeThread(main_thread);
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
void AlphaBlend(unsigned char *src, unsigned char *dst, int width, int height, int row)
{
    int index = 0;
    int src_alpha;
    int one_minus_src_alpha;
    for (int i = 0; i < height; i++, index += row)
    {
        for (int j = 0; j < width; j++)
        {
            int off = index + (j << 2);
            src_alpha = (int)(src[off + 3]) & 0xFF;
            if (src_alpha == 0xFF)
            {
                dst[off + 0] = src[off + 0];
                dst[off + 1] = src[off + 1];
                dst[off + 2] = src[off + 2];
                continue;
            }
            else if (!src_alpha)
            {
                continue;
            }
            one_minus_src_alpha = 0xFF - src_alpha;
            dst[off + 0] = (char)((((((int)(src[off + 0]) & 0xFF) * src_alpha) + ((int)(dst[off + 0]) & 0xFF) * one_minus_src_alpha) / 0xFF) & 0xFF);
            dst[off + 1] = (char)((((((int)(src[off + 1]) & 0xFF) * src_alpha) + ((int)(dst[off + 1]) & 0xFF) * one_minus_src_alpha) / 0xFF) & 0xFF);
            dst[off + 2] = (char)((((((int)(src[off + 2]) & 0xFF) * src_alpha) + ((int)(dst[off + 2]) & 0xFF) * one_minus_src_alpha) / 0xFF) & 0xFF);
        }
    }
}

void PrepareOutline()
{
    size_t len = (VERTEX_ATTR_LENGTH_DIRECTCOLOR * 24) + 3 + 32;
    if ((len & 31) != 0)
    {
        len += 32;
        len &= ~31;
    }
    void *ret = memalign(32, len);
    if (!ret)
        return;
    GX_SetLineWidth(24, 0);
    GX_BeginDispList(ret, len);
    GX_BeginGroup(GX_LINES, 24);
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(vertex_property_t(vec3f(k, i, j), 0, 0, 0, 0, 0));
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(vertex_property_t(vec3f(i, k, j), 0, 0, 0, 0, 0));
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Vertex(vertex_property_t(vec3f(i, j, k), 0, 0, 0, 0, 0));
            }
        }
    }
    GX_EndGroup();
    outline_len = GX_EndDispList();
    DCInvalidateRange(ret, len);
    outline = ret;
}

int main(int argc, char **argv)
{
    u32 fb = 0;
    f32 yscale;
    u32 xfbHeight;
    Mtx view; // view and perspective matrices
    Mtx44 perspective;
    void *gpfifo = NULL;
    GXColor background = get_sky_color();

    threadqueue_init();

    VIDEO_Init();
    WPAD_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    SYS_SetPowerCallback(WiiPowerPressed);
    SYS_SetResetCallback(WiiResetPressed);
    WPAD_SetPowerButtonCallback(WiimotePowerPressed);
    // allocate the fifo buffer
    gpfifo = memalign(32, DEFAULT_FIFO_SIZE);
    memset(gpfifo, 0, DEFAULT_FIFO_SIZE);

    // allocate 2 framebuffers for double buffering
    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    // dstBuffer[0] = SYS_AllocateFramebuffer(rmode);
    // dstBuffer[1] = SYS_AllocateFramebuffer(rmode);
    CON_Init(frameBuffer[0], 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    // configure video
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
    fb ^= 1;
    // init the flipper
    GX_Init(gpfifo, DEFAULT_FIFO_SIZE);

    // clears the bg to color and clears the z buffer
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

    // setup the vertex attribute table
    // describes the data
    // args: vat location 0-7, type of data, data format, size, scale
    // so for ex. in the first call we are sending position data with
    // 3 values X,Y,Z of size F32. scale sets the number of fractional
    // bits for non float data.
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_NRM, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, BASE3D_UV_FRAC_BITS);
    // set number of rasterized color channels
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0, DIRECTIONAL_LIGHT, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT0, GX_DF_CLAMP, GX_AF_SPOT);
    GX_SetChanAmbColor(GX_COLOR0, GXColor{0, 0, 0, 255});
    GX_SetChanMatColor(GX_COLOR0, GXColor{255, 255, 255, 255});
    //  set number of textures to generate
    GX_SetNumTexGens(1);

    GX_InvVtxCache();
    GX_InvalidateTexAll();

    init_textures();

    uint32_t texture_buflen = GX_GetTexBufferSize(GX_GetTexObjWidth(&blockmap_texture), GX_GetTexObjHeight(&blockmap_texture), GX_GetTexObjFmt(&blockmap_texture), GX_FALSE, GX_FALSE);
    texanim_t water_still_anim;
    extract_texanim_info(water_still_anim, water_still_texture, blockmap_texture);
    water_still_anim.tile_width = 16;
    water_still_anim.tile_height = 16;
    water_still_anim.dst_x = 208;
    water_still_anim.dst_y = 192;

    // setup our camera at the origin
    // looking down the -z axis with y up
    guVector cam = {0.0F, 0.0F, 0.0F},
             up = {0.0F, 1.0F, 0.0F},
             look = {0.0F, 0.0F, -1.0F};
    guLookAt(view, &cam, &up, &look);
    // setup our projection matrix
    // this creates a perspective matrix with a view angle of 90,
    // and aspect ratio based on the display resolution
    f32 w = rmode->fbWidth;
    f32 h = rmode->efbHeight;
    s32 ar = CONF_GetAspectRatio();

    if (ar == CONF_ASPECT_16_9)
    {
        h *= 0.825f;
    }
    else
    {
        h *= 1 / 0.9f;
    }

    f32 FOV = 90;

    camera_t camera = {
        {0.0f, 0.0f, 0.0f},           // Camera position
        {0.0f, 0.0f, -1.0f},          // Camera forward vector
        FOV,                          // Field of view
        w / h,                        // Aspect ratio
        0.1f,                         // Near clipping plane
        (RENDER_DISTANCE + 1) * 16.0f // Far clipping plane
    };
    fatInitDefault();
    printf("Render resolution: %f,%f, Widescreen: %s\n", w, h, ar == CONF_ASPECT_16_9 ? "Yes" : "No");
    light_engine_init();
    printf("Initialized basics.\n");
    VIDEO_Flush();
    VIDEO_WaitVSync();
    srand(gettime());
    printf("Initialized rng.\n");
    VIDEO_Flush();
    VIDEO_WaitVSync();
    init_chunks();
    printf("Initialized chunks.\n");
    VIDEO_Flush();
    VIDEO_WaitVSync();
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    PrepareOutline();
    GX_SetZCompLoc(GX_FALSE);
    player_pos.x = 0;
    player_pos.y = -1000;
    player_pos.z = 0;
    VIDEO_SetPostRetraceCallback(&RenderDone);
    main_thread = LWP_GetSelf();
    init_face_normals();
    PrepareTEV();
    std::deque<chunk_t *> &chunks = get_chunks();
    while (!isExiting)
    {
        u64 frame_start = time_get();
        float sky_multiplier = get_sky_multiplier();
        if (HWButton != -1)
            break;
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
        LightMapBlend(light_day_rgba, light_night_rgba, light_map, 255 - uint8_t(sky_multiplier * 255));
        GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));
        GX_InvVtxCache();
        background = get_sky_color();
        GX_SetCopyClear(background, 0x00FFFFFF);
        GX_SetFog(GX_FOG_PERSP_LIN, RENDER_DISTANCE * 0.67f * 16 - 16, RENDER_DISTANCE * 0.67f * 16 - 8, 0.1F, 3000.0F, background);
        if (player_pos.y < -999)
            player_pos.y = skycast(vec3i(int(player_pos.x), 0, int(player_pos.z))) + 2;
        UpdateLightDir();
        if (fb)
        {
            water_still_anim.update();
            DCFlushRange(water_still_anim.target, texture_buflen);
            GX_InvalidateTexAll();
        }
        use_texture(blockmap_texture);
        GetInput();

        guVector forward = angles_to_vector(xrot, yrot, -1);

        camera.position[0] = player_pos.x;
        camera.position[1] = player_pos.y;
        camera.position[2] = player_pos.z;

        camera.forward[0] = forward.x;
        camera.forward[1] = forward.y;
        camera.forward[2] = forward.z;

        // Construct the view frustum matrix from the camera
        frustum_t frustum = calculate_frustum(camera);

        UpdateScene(chunks);

        // Prepare projection matrix for rendering the world
        guPerspective(perspective, FOV, (f32)w / h, 0.1F, 3000.0F);
        GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

        // Enable backface culling for terrain
        GX_SetCullMode(GX_CULL_BACK);

        // Draw opaque buffer
        GX_SetZMode(GX_TRUE, GX_LESS, GX_TRUE);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
        GX_SetAlphaUpdate(GX_TRUE);
        GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
        GX_SetColorUpdate(GX_TRUE);
        DrawScene(view, frustum, chunks, false);
        // Draw transparent buffer
        GX_SetZMode(GX_TRUE, GX_LESS, GX_TRUE);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
        GX_SetAlphaUpdate(GX_FALSE);
        GX_SetAlphaCompare(GX_GEQUAL, 16, GX_AOP_AND, GX_ALWAYS, 0);
        GX_SetColorUpdate(GX_TRUE);
        DrawScene(view, frustum, chunks, true);
        // Draw sky
        draw_sky(view, background);

        // Prepare projection matrix for rendering GUI elements
        guOrtho(perspective, 0, h - 1, 0, w - 1, 0, 3000);
        GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

        // Prepare position matrix for rendering GUI elements
        Mtx flat_matrix;
        guMtxIdentity(flat_matrix);
        guMtxTransApply(flat_matrix, flat_matrix, 0.0F, 0.0F, -0.5F);
        GX_LoadPosMtxImm(flat_matrix, GX_PNMTX0);

        // Use 0 fractional bits for the position data, because we're drawing in pixel space.
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);

        // Disable fog
        GX_SetFog(GX_FOG_NONE, RENDER_DISTANCE * 0.67f * 16 - 16, RENDER_DISTANCE * 0.67f * 16 - 8, 0.1F, 3000.0F, background);

        // Draw GUI elements
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
        GX_SetAlphaUpdate(GX_TRUE);
        GX_SetColorUpdate(GX_TRUE);

        draw_simple_textured_quad(blockmap_texture, 16, 32, 256, 256);

        GX_DrawDone();

        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        frameCounter++;
        deltaTime = time_diff_s(frame_start, time_get());
        partialTicks += deltaTime * 20;
        if (partialTicks >= 1.0f)
            tickCounter += int(partialTicks);
        partialTicks -= int(partialTicks);
        fb ^= 1;
    }
    printf("De-initializing light engine...\n");
    light_engine_deinit();
    printf("Notifying threads...\n");
    threadqueue_broadcast();
    printf("De-initializing chunk engine...\n");
    deinit_chunks();
    printf("Exiting...");
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (HWButton != -1)
    {
        SYS_ResetSystem(HWButton, 0, 0);
    }
    return 0;
}

void GetInput()
{
    WPAD_ScanPads();
    u32 wiimote1_down = WPAD_ButtonsDown(0);
    u32 wiimote1_held = WPAD_ButtonsHeld(0);
    if ((wiimote1_down & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME)))
        isExiting = true;
    if ((wiimote1_down & WPAD_BUTTON_1))
    {
        printf("PRIM_CHUNK_MEMORY: %d B\n", total_chunks_size);
    }
    if ((wiimote1_down & WPAD_BUTTON_2))
    {
        tickCounter += 6000;
    }

    expansion_t expansion;
    WPAD_Expansion(0, &expansion);
    if (expansion.type == WPAD_EXP_CLASSIC)
    {
        int x = expansion.classic.rjs.pos.x;
        int y = expansion.classic.rjs.pos.y;
        y -= expansion.classic.rjs.center.y;
        x -= expansion.classic.rjs.center.x;
        if (std::abs(x) > CLASSIC_CONTROLLER_THRESHOLD)
        {
            yrot -= (float(x) / (expansion.classic.rjs.max.x - expansion.classic.rjs.min.x)) * LOOKAROUND_SENSITIVITY * deltaTime;
        }
        if (std::abs(y) > CLASSIC_CONTROLLER_THRESHOLD)
        {
            xrot += (float(y) / (expansion.classic.rjs.max.y - expansion.classic.rjs.min.y)) * LOOKAROUND_SENSITIVITY * deltaTime;
        }

        if (wiimote1_held & WPAD_CLASSIC_BUTTON_UP)
            player_pos.y += MOVEMENT_SPEED * deltaTime;
        if (wiimote1_held & WPAD_CLASSIC_BUTTON_DOWN)
            player_pos.y -= MOVEMENT_SPEED * deltaTime;

        if (!((wiimote1_held & WPAD_CLASSIC_BUTTON_FULL_L) || (wiimote1_held & WPAD_CLASSIC_BUTTON_FULL_R)))
            shoulder_btn_frame_counter = -1;
        else
            shoulder_btn_frame_counter++;

        place_block = false;
        destroy_block = false;
        if (shoulder_btn_frame_counter >= 0)
        {
            // repeats buttons every 10 frames
            if ((wiimote1_held & WPAD_CLASSIC_BUTTON_FULL_L) && ((wiimote1_down & WPAD_CLASSIC_BUTTON_FULL_L) || shoulder_btn_frame_counter % 10 == 0))
                place_block = true;
            if ((wiimote1_held & WPAD_CLASSIC_BUTTON_FULL_R) && ((wiimote1_down & WPAD_CLASSIC_BUTTON_FULL_R) || shoulder_btn_frame_counter % 10 == 0))
                place_block = !(destroy_block = true);
        }

        x = expansion.classic.ljs.pos.x;
        y = expansion.classic.ljs.pos.y;
        y -= expansion.classic.ljs.center.y;
        x -= expansion.classic.ljs.center.x;
        if (std::abs(x) > CLASSIC_CONTROLLER_THRESHOLD)
        {
            player_pos.x += (float(x) / (expansion.classic.ljs.max.y - expansion.classic.ljs.min.y)) * sin(DegToRad(yrot + 90)) * MOVEMENT_SPEED * deltaTime;
            player_pos.z += (float(x) / (expansion.classic.ljs.max.y - expansion.classic.ljs.min.y)) * cos(DegToRad(yrot + 90)) * MOVEMENT_SPEED * deltaTime;
        }
        if (std::abs(y) > CLASSIC_CONTROLLER_THRESHOLD)
        {
            player_pos.x -= (float(y) / (expansion.classic.ljs.max.y - expansion.classic.ljs.min.y)) * sin(DegToRad(yrot)) * MOVEMENT_SPEED * deltaTime;
            player_pos.z -= (float(y) / (expansion.classic.ljs.max.y - expansion.classic.ljs.min.y)) * cos(DegToRad(yrot)) * MOVEMENT_SPEED * deltaTime;
        }
    }
    if (yrot > 360.f)
        yrot -= 360.f;
    if (yrot < 0)
        yrot += 360.f;
    if (xrot > 90.f)
        xrot = 90.f;
    if (xrot < -90.f)
        xrot = -90.f;
}

void GetLookMtx(Mtx &mtx)
{
    guVector axis; // Axis to rotate on
    Mtx rotx;
    Mtx roty;
    // Rotate by view
    axis.x = 1;
    axis.y = 0;
    axis.z = 0;
    guMtxRotAxisDeg(rotx, &axis, xrot);
    axis.x = 0;
    axis.y = 1;
    axis.z = 0;
    guMtxRotAxisDeg(roty, &axis, yrot);
    guMtxConcat(rotx, roty, mtx);
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

void Render(Mtx view, guVector chunkPos, void *buffer, u32 length)
{
    if (!buffer || !length)
        return;
    transform_view(view, chunkPos);
    // Render
    GX_CallDispList(ALIGNPTR(buffer), length); // Draw the box
}

inline void RecalcSectionWater(chunk_t *chunk, int section)
{
    static std::vector<std::vector<vec3i>> fluid_levels(9);

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
                if (block && (block->meta & FLUID_UPDATE_REQUIRED_FLAG))
                    fluid_levels[get_fluid_meta_level(block)].push_back(current_pos);
            }
        }
    }
    // for (int i = fluid_levels.size() - 1; i >= 0; i--)
    for (size_t i = 0; i < fluid_levels.size(); i++)
    {
        std::vector<vec3i> &positions = fluid_levels[i];
        for (vec3i pos : positions)
        {
            block_t *block = chunk->get_block(pos);
            update_fluid(block, pos);
        }
        positions.clear();
    }
}

void GenerateAdditionalChunks(std::deque<chunk_t *> &chunks)
{
    if (chunks.size() >= CHUNK_COUNT)
    {
        return;
    }
    for (int x = player_pos.x - GENERATION_DISTANCE; x < player_pos.x + GENERATION_DISTANCE; x += 8)
    {
        int distance = std::abs(x - int(player_pos.x));
        if (distance > GENERATION_DISTANCE)
            return;
        for (int z = player_pos.z - GENERATION_DISTANCE; z < player_pos.z + GENERATION_DISTANCE; z += 8)
        {
            distance = std::abs(z - int(player_pos.z));
            if (distance > GENERATION_DISTANCE)
                return;
            if (!get_chunk_from_pos(vec3i(x, 0, z), true, false))
            {
                threadqueue_broadcast();
                return;
            }
        }
    }
}

void RemoveRedundantChunks(std::deque<chunk_t *> &chunks)
{
    lock_chunks();
    chunks.erase(
        std::remove_if(chunks.begin(), chunks.end(),
                       [](chunk_t *&c)
                       {if(!c) return true; if(!c->valid) {delete c; c = nullptr; return true;} return false; }),
        chunks.end());
    unlock_chunks();
}

void PrepareChunkRemoval(chunk_t *chunk)
{
    chunk->valid = false;
    for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
    {
        chunkvbo_t &vbo = chunk->vbos[j];
        vbo.visible = false;
        vbo.solid_buffer = nullptr;
        vbo.solid_buffer_length = 0;
        vbo.transparent_buffer = nullptr;
        vbo.transparent_buffer_length = 0;
        if (vbo.cached_solid_buffer && vbo.cached_solid_buffer_length)
        {
            free(vbo.cached_solid_buffer);
            vbo.cached_solid_buffer = nullptr;
            vbo.cached_solid_buffer_length = 0;
        }
        if (vbo.cached_transparent_buffer && vbo.cached_transparent_buffer_length)
        {
            free(vbo.cached_transparent_buffer);
            vbo.cached_transparent_buffer = nullptr;
            vbo.cached_transparent_buffer_length = 0;
        }
    }
}

void EditBlocks()
{
    guVector forward = angles_to_vector(xrot, yrot, -1);

    draw_block_outline = raycast(vec3d{player_pos.x - .5, player_pos.y - .5, player_pos.z - .5}, vec3d{forward.x, forward.y, forward.z}, 4, &raycast_block, &raycast_block_face);
    if (draw_block_outline)
    {
        BlockID target_blockid = destroy_block ? BlockID::air : BlockID::torch;
        if (destroy_block || place_block)
        {
            vec3i editable_pos = destroy_block ? (raycast_block) : (raycast_block + raycast_block_face);
            block_t *editable_block = get_block_at(editable_pos);
            if (place_block)
                update_block_at(editable_pos);
            for (int i = 0; i < 6; i++)
                update_block_at(editable_pos + face_offsets[i]);
            if (editable_block)
            {
                editable_block->set_blockid(target_blockid);
                editable_block->meta = 0;
                update_block_at(raycast_block);
                block_t *neighbors[6];
                get_neighbors(editable_pos, neighbors);
                for (int i = 0; i < 6; i++)
                {
                    if (neighbors[i] && is_fluid(neighbors[i]->get_blockid()))
                        neighbors[i]->meta |= FLUID_UPDATE_REQUIRED_FLAG;
                }
            }
        }
    }

    // Clear the place/destroy block flags to prevent placing blocks immediately.
    place_block = false;
    destroy_block = false;
}

void UpdateChunkData(std::deque<chunk_t *> &chunks)
{
    int light_up_calls = 0;
    for (chunk_t *&chunk : chunks)
    {
        if (chunk && chunk->valid)
        {
            float distance = std::max(std::abs((chunk->x * 16) - player_pos.x), std::abs((chunk->z * 16) - player_pos.z));
            if (distance > RENDER_DISTANCE * 16)
            {
                PrepareChunkRemoval(chunk);
                continue;
            }

            // Tick chunks
            for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
            {
                chunkvbo_t &vbo = chunk->vbos[j];
                distance = std::abs((j * 16) - player_pos.y);
                vbo.visible = (distance <= RENDER_DISTANCE * 16);
                if (distance <= SIMULATION_DISTANCE * 16 && tickCounter % 4 == 0)
                    RecalcSectionWater(chunk, j);
            }
            if (!chunk->lit_state && light_up_calls < 1)
            {
                light_up_calls++;
                chunk->light_up();
            }
        }
    }
}

void UpdateChunkVBOs(std::deque<chunk_t *> &chunks)
{
    int chunk_update_count = 0;
    for (chunk_t *&chunk : chunks)
    {
        if (!chunk || chunk_update_count >= 1)
            continue;
        if (chunk->valid && !chunk->light_updates)
        {
            bool chunk_updated = false;
            for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
            {
                if (light_engine_busy())
                    return;
                chunkvbo_t &vbo = chunk->vbos[j];
                if (!vbo.visible)
                    continue;
                if (vbo.dirty && !chunk->light_updates)
                {
                    vbo.dirty = false;
                    chunk->recalculate_section(j);
                    chunk->build_vbo(j, false);
                    chunk->build_vbo(j, true);
                    chunk_updated = true;
                }
                if (vbo.solid_buffer != vbo.cached_solid_buffer)
                {
                    if (vbo.cached_solid_buffer && vbo.cached_solid_buffer_length)
                    {
                        free(vbo.cached_solid_buffer);
                    }
                    vbo.cached_solid_buffer = vbo.solid_buffer;
                    vbo.cached_solid_buffer_length = vbo.solid_buffer_length;
                }
                if (vbo.transparent_buffer != vbo.cached_transparent_buffer)
                {
                    if (vbo.cached_transparent_buffer && vbo.cached_transparent_buffer_length)
                    {
                        free(vbo.cached_transparent_buffer);
                    }
                    vbo.cached_transparent_buffer = vbo.transparent_buffer;
                    vbo.cached_transparent_buffer_length = vbo.transparent_buffer_length;
                }
            }
            chunk_update_count += chunk_updated;
            if (chunk->lit_state)
                chunk->lit_state = 2;
        }
    }
}

void UpdateScene(std::deque<chunk_t *> &chunks)
{
    if (!light_engine_busy())
        GenerateAdditionalChunks(chunks);
    RemoveRedundantChunks(chunks);
    EditBlocks();
    UpdateChunkData(chunks);
    UpdateChunkVBOs(chunks);

    // Calculate chunk memory usage
    total_chunks_size = 0;
    for (chunk_t *&chunk : chunks)
    {
        total_chunks_size += chunk ? chunk->size() : 0;
    }
}

void DrawScene(Mtx view, frustum_t &frustum, std::deque<chunk_t *> &chunks, bool transparency)
{
    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Draw the solid pass
    if (!transparency)
    {
        for (chunk_t *&chunk : chunks)
        {
            if (chunk && chunk->valid && chunk->lit_state == 2)
            {
                for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
                {
                    chunkvbo_t &vbo = chunk->vbos[j];
                    if (!vbo.visible)
                        continue;

                    guVector chunkPos = {(f32)chunk->x * 16, (f32)j * 16, (f32)chunk->z * 16};
                    vec3f chunkCenter(chunkPos.x + 8, chunkPos.y + 8, chunkPos.z + 8);

                    if (distance_to_frustum(chunkCenter, frustum) < 14 && vbo.cached_solid_buffer && vbo.cached_solid_buffer_length)
                    {
                        Render(view, chunkPos, vbo.cached_solid_buffer, vbo.cached_solid_buffer_length);
                    }
                }
            }
        }
    }
    // Draw the transparent pass
    else
    {
        for (chunk_t *&chunk : chunks)
        {
            if (chunk && chunk->valid && chunk->lit_state == 2)
            {
                for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
                {
                    chunkvbo_t &vbo = chunk->vbos[j];
                    if (!vbo.visible)
                        continue;

                    guVector chunkPos = {(f32)chunk->x * 16, (f32)j * 16, (f32)chunk->z * 16};
                    vec3f chunkCenter(chunkPos.x + 8, chunkPos.y + 8, chunkPos.z + 8);

                    if (distance_to_frustum(chunkCenter, frustum) < 14 && vbo.cached_transparent_buffer && vbo.cached_transparent_buffer_length)
                    {
                        Render(view, chunkPos, vbo.cached_transparent_buffer, vbo.cached_transparent_buffer_length);
                    }
                }
            }
        }
    }

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Draw block outlines
    if (transparency && outline && draw_block_outline)
    {
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        guVector outlinePos = guVector();
        outlinePos.x = raycast_block.x - .5;
        outlinePos.y = raycast_block.y - .5;
        outlinePos.z = raycast_block.z - .5;
        Render(view, outlinePos, outline, outline_len);

        // Draw debug spritesheet
        if (debug_spritesheet)
        {
            transform_view(view, outlinePos);
            GX_BeginGroup(GX_QUADS, 4);
            GX_Vertex(vertex_property_t(vec3f(-2.0f, 1.1f, -2.0f), 0, 0));
            GX_Vertex(vertex_property_t(vec3f(2.0f, 1.1f, -2.0f), 128, 0));
            GX_Vertex(vertex_property_t(vec3f(2.0f, 1.1f, 2.0f), 128, 128));
            GX_Vertex(vertex_property_t(vec3f(-2.0f, 1.1f, 2.0f), 0, 128));
            GX_EndGroup();
        }
    }
}
void PrepareTEV()
{
    GX_SetNumTevStages(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
}
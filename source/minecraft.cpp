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
#include "chunk_new.hpp"
#include "block.hpp"
#include "blocks.hpp"
#include "blockmap_tpl.h"
#include "blockmap.h"
#include "water_still_tpl.h"
#include "water_still.h"
#include "vec3i.hpp"
#include "vec3d.hpp"
#include "timers.hpp"
#include "texturedefs.h"
#include "cpugx.hpp"
#include "raycast.hpp"
#include "light.hpp"
#include "texanim.hpp"
#define DEFAULT_FIFO_SIZE (256 * 1024)
#define CLASSIC_CONTROLLER_THRESHOLD 4
#define MAX_PARTICLES 100
#define LOOKAROUND_SENSITIVITY 5;

float lerp(float a, float b, float f)
{
    return a * (1.0 - f) + (b * f);
}

f32 xrot = -22.5f;
f32 yrot = 0.0f;
guVector pos = {0.F, 80.0F, 0.F};
void *frameBuffer[2] = {NULL, NULL};
// static void *dstBuffer[2] = {NULL, NULL};
static GXRModeObj *rmode = NULL;

void *outline = nullptr;
u32 outline_len = 0;

int dropFrames = 0;

int frameCounter = 0;
int tickCounter = 0;

int shoulder_btn_frame_counter = 0;
float prev_left_shoulder = 0;
float prev_right_shoulder = 0;
bool destroy_block = false;
bool place_block = false;

bool DrawScene(Mtx view, bool transparency);
void PrepareTexture(GXTexObj texture);
void GetInput();
guVector cam = {0.0F, 0.0F, 0.0F},
         up = {0.0F, 1.0F, 0.0F},
         look = {0.0F, 0.0F, 1.0F};
//---------------------------------------------------------------------------------
bool isExiting = false;
void PowerOffCallback()
{
    isExiting = true;
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
    size_t len = (VERTEX_ATTR_LENGTH * 24) + 3 + 32;
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
    GX_Begin(GX_LINES, GX_VTXFMT0, 24);
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Position3f32(k, i, j);
                GX_Color3f32(0, 0, 0);
                GX_TexCoord2u8(0, 0);
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Position3f32(i, k, j);
                GX_Color3f32(0, 0, 0);
                GX_TexCoord2u8(0, 0);
            }
        }
    }
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                GX_Position3f32(i, j, k);
                GX_Color3f32(0, 0, 0);
                GX_TexCoord2u8(0, 0);
            }
        }
    }
    GX_End();
    outline_len = GX_EndDispList();
    DCInvalidateRange(ret, len);
    outline = ret;
}

void ExtractTPLInfo(texanim_t &anim, bool dst, TPLFile *tpl, int index = 0)
{
    uint32_t fmt;
    uint16_t w;
    uint16_t h;
    TPL_GetTextureInfo(tpl, index, &fmt, &w, &h);
    if (dst)
    {
        anim.dst_format = fmt;
        anim.dst_width = w;
    }
    else
    {
        anim.src_format = fmt;
        anim.src_width = w;
        anim.src_height = h;
    }
}

int main(int argc, char **argv)
{

    u32 fb = 0;
    f32 yscale;
    u32 xfbHeight;
    Mtx view; // view and perspective matrices
    Mtx44 perspective;
    void *gpfifo = NULL;
    GXColor default_background = {0x8D, 0xBB, 0xFF, 0x00};
    GXColor background = default_background;
    float current_lerpvalue = 1.f;

    TPLFile blockmapTPL;
    TPLFile water_stillTPL;
    GXTexObj texture;
    GXTexObj water_still_texture;

    VIDEO_Init();
    WPAD_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    SYS_SetPowerCallback(PowerOffCallback);
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

    GX_SetCullMode(GX_CULL_BACK);
    // GX_SetCullMode(GX_CULL_NONE);
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
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
#ifdef OPTIMIZE_UVS
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U8, GX_ITS_128);
#else
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
#endif
    // set number of rasterized color channels
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    // set number of textures to generate
    GX_SetNumTexGens(1);

    GX_InvVtxCache();
    GX_InvalidateTexAll();

    // Water still texture
    TPL_OpenTPLFromMemory(&water_stillTPL, (void *)water_still_tpl, water_still_tpl_size);
    TPL_GetTexture(&water_stillTPL, water_still, &water_still_texture);
    GX_InitTexObjFilterMode(&water_still_texture, GX_NEAR, GX_NEAR);
    // Terrain texture
    TPL_OpenTPLFromMemory(&blockmapTPL, (void *)blockmap_tpl, blockmap_tpl_size);
    TPL_GetTexture(&blockmapTPL, blockmap, &texture);
    GX_InitTexObjFilterMode(&texture, GX_NEAR, GX_NEAR);

    uint32_t texture_buflen = GX_GetTexBufferSize(GX_GetTexObjWidth(&texture), GX_GetTexObjHeight(&texture), GX_GetTexObjFmt(&texture), GX_FALSE, GX_FALSE);
    void *texture_ptr = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&texture));

    texanim_t water_still_anim;
    water_still_anim.source = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&water_still_texture));
    water_still_anim.target = texture_ptr;
    ExtractTPLInfo(water_still_anim, TA_SRC, &water_stillTPL, water_still);
    ExtractTPLInfo(water_still_anim, TA_DST, &blockmapTPL, blockmap);
    water_still_anim.tile_width = 16;
    water_still_anim.tile_height = 16;
    water_still_anim.dst_x = 208;
    water_still_anim.dst_y = 192;

    // setup our camera at the origin
    // looking down the -z axis with y up
    guLookAt(view, &cam, &up, &look);
    // setup our projection matrix
    // this creates a perspective matrix with a view angle of 90,
    // and aspect ratio based on the display resolution
    f32 w = rmode->viWidth;
    f32 h = rmode->viHeight;
    guPerspective(perspective, 90, (f32)w / h, 0.1F, 300.0F);
    GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);
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
    PrepareOutline();
    GX_SetFog(GX_FOG_LIN, 8, 22, 0.1F, 300.0F, background);
    GX_SetZCompLoc(GX_FALSE);
    time_reset();
    while (!isExiting)
    {
        if (fb)
        {
            water_still_anim.update();
            DCFlushRange(texture_ptr, texture_buflen);
            GX_InvalidateTexAll();
            PrepareTexture(texture);
        }
        GetInput();
        // u64 frame_start = time_get();

        // Draw opaque buffer
        GX_SetZMode(GX_TRUE, GX_LESS, GX_TRUE);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
        GX_SetAlphaUpdate(GX_TRUE);
        // GX_SetAlphaCompare(GX_EQUAL, 255, GX_AOP_AND, GX_ALWAYS, 0);
        GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
        GX_SetColorUpdate(GX_TRUE);
        DrawScene(view, false);
        // Draw transparent buffer
        GX_SetZMode(GX_TRUE, GX_LESS, GX_TRUE);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
        GX_SetAlphaUpdate(GX_FALSE);
        // GX_SetAlphaCompare(GX_LESS, 255, GX_AOP_AND, GX_ALWAYS, 0); // 1st param: GX_LESS
        GX_SetAlphaCompare(GX_GEQUAL, 16, GX_AOP_AND, GX_ALWAYS, 0);
        GX_SetColorUpdate(GX_TRUE);
        DrawScene(view, true);
        GX_DrawDone();
        // bool frame_skip = time_diff_ms(frame_start, time_get()) > 20;
        // if (frame_skip)
        //     GX_AbortFrame();
        // else
        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        // AlphaBlend((unsigned char *)frameBuffer, (unsigned char *)(dstBuffer[fb]), rmode->fbWidth, xfbHeight, rmode->fbWidth);
        //  Update framebuffer
        //  VIDEO_SetNextFramebuffer(dstBuffer[fb]);
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        frameCounter++;
        VIDEO_WaitVSync();
        // if (!frame_skip)
        fb ^= 1;
        float target = pos.y >= 64. ? 1 : (pos.y <= 63. ? 0 : std::fmod(pos.y, 1));
        target = target < 0.1f ? 0.1f : target;
        current_lerpvalue = lerp(current_lerpvalue, target, 0.05f);
        background.r = u8(current_lerpvalue * default_background.r);
        background.g = u8(current_lerpvalue * default_background.g);
        background.b = u8(current_lerpvalue * default_background.b);
        GX_SetCopyClear(background, 0x00FFFFFF);
        GX_SetFog(GX_FOG_LIN, 8, 22, 0.1F, 300.0F, background);
    }
    light_engine_deinit();
    printf("Exiting...");
    VIDEO_Flush();
    VIDEO_WaitVSync();
    SYS_ResetSystem(SYS_POWEROFF, 0, 0);
    // while (1)
    //     ;
    return 0;
}

void GetInput()
{
    WPAD_ScanPads();
    u32 wiimote1_down = WPAD_ButtonsDown(0);
    u32 wiimote1_held = WPAD_ButtonsHeld(0);
    if ((wiimote1_down & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME)))
        isExiting = true;

    expansion_t expansion;
    WPAD_Expansion(0, &expansion);
    if (expansion.type == WPAD_EXP_CLASSIC)
    {
        int x = expansion.classic.rjs.pos.x;
        int y = expansion.classic.rjs.pos.y;
        y -= expansion.classic.rjs.center.y;
        x -= expansion.classic.rjs.center.x;
        if (abs(x) > CLASSIC_CONTROLLER_THRESHOLD)
        {
            yrot -= (float(x) / (expansion.classic.rjs.max.x - expansion.classic.rjs.min.x)) * LOOKAROUND_SENSITIVITY;
        }
        if (abs(y) > CLASSIC_CONTROLLER_THRESHOLD)
        {
            xrot += (float(y) / (expansion.classic.rjs.max.y - expansion.classic.rjs.min.y)) * LOOKAROUND_SENSITIVITY;
        }

        if (wiimote1_held & WPAD_CLASSIC_BUTTON_UP)
            pos.y += 0.35f;
        if (wiimote1_held & WPAD_CLASSIC_BUTTON_DOWN)
            pos.y -= 0.35f;

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
        if (abs(x) > CLASSIC_CONTROLLER_THRESHOLD)
        {
            pos.x += (float(x) / (expansion.classic.ljs.max.y - expansion.classic.ljs.min.y)) * sin(DegToRad(yrot + 90)) * 0.35f;
            pos.z += (float(x) / (expansion.classic.ljs.max.y - expansion.classic.ljs.min.y)) * cos(DegToRad(yrot + 90)) * 0.35f;
        }
        if (abs(y) > CLASSIC_CONTROLLER_THRESHOLD)
        {
            pos.x -= (float(y) / (expansion.classic.ljs.max.y - expansion.classic.ljs.min.y)) * sin(DegToRad(yrot));
            pos.z -= (float(y) / (expansion.classic.ljs.max.y - expansion.classic.ljs.min.y)) * cos(DegToRad(yrot));
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
guVector RotateVector(guVector vec, float x, float y, float z)
{
    x = DegToRad(x);
    y = DegToRad(y);
    z = DegToRad(z);
    float cosa = cos(x);
    float sina = sin(x);

    float cosb = cos(y);
    float sinb = sin(y);

    float cosc = cos(z);
    float sinc = sin(z);

    float Axx = cosa * cosb;
    float Axy = cosa * sinb * sinc - sina * cosc;
    float Axz = cosa * sinb * cosc + sina * sinc;

    float Ayx = sina * cosb;
    float Ayy = sina * sinb * sinc + cosa * cosc;
    float Ayz = sina * sinb * cosc - cosa * sinc;

    float Azx = -sinb;
    float Azy = cosb * sinc;
    float Azz = cosb * cosc;
    float px = vec.x;
    float py = vec.y;
    float pz = vec.z;

    vec.x = Axx * px + Axy * py + Axz * pz;
    vec.y = Ayx * px + Ayy * py + Ayz * pz;
    vec.z = Azx * px + Azy * py + Azz * pz;
    return vec;
}

guVector RotateVectorAround(guVector vec, float x, float y, float z, guVector around = guVector())
{
    guVector toRotate;
    guVecSub(&vec, &around, &toRotate);
    toRotate = RotateVector(toRotate, x, y, z);
    guVecAdd(&toRotate, &around, &toRotate);
    return toRotate;
}

guVector AnglesToVector(float x, float y, float distance, guVector vec = guVector())
{
    vec.x += cos(DegToRad(x)) * sin(DegToRad(y));
    vec.y += -sin(DegToRad(x));
    vec.z += cos(DegToRad(x)) * cos(DegToRad(y));
    /*
    vec.x += cos(DegToRad(x)) * cos(DegToRad(y)) * distance;
    vec.z += sin(DegToRad(x)) * cos(DegToRad(y)) * distance;
    vec.y += sin(DegToRad(y)) * distance;
     */
    return vec;
}

void Transform(Mtx view, guVector chunkPos)
{
    Mtx model, modelview;
    Mtx offset;
    Mtx posmtx;
    Mtx rotx;
    Mtx roty;
    Mtx rotz;
    guVector axis; // Axis to rotate on

    // Reset matrices
    guMtxIdentity(model);
    guMtxIdentity(offset);
    guMtxIdentity(posmtx);
    guMtxIdentity(rotz);
    guMtxIdentity(roty);
    guMtxIdentity(rotx);

    // Position the chunks on the screen
    guMtxTrans(offset, chunkPos.x, -chunkPos.y, chunkPos.z);
    guMtxTrans(posmtx, pos.x, pos.y, pos.z);

    // Rotate view
    axis.x = 1;
    axis.y = 0;
    axis.z = 0;
    guMtxRotAxisDeg(rotx, &axis, xrot);
    axis.x = 0;
    axis.y = 1;
    axis.z = 0;
    guMtxRotAxisDeg(roty, &axis, yrot);

    // Apply matrices
    guMtxConcat(posmtx, offset, offset);
    guMtxConcat(offset, rotz, rotz);
    guMtxConcat(rotz, roty, roty);
    guMtxConcat(roty, rotx, model);
    guMtxInverse(model, model);
    guMtxConcat(model, view, modelview);
    GX_LoadPosMtxImm(modelview, GX_PNMTX0);
}

void Render(Mtx view, guVector chunkPos, void *buffer, u32 length)
{
    if (!buffer || !length)
        return;
    Transform(view, chunkPos);
    // Render
    GX_CallDispList(ALIGNPTR(buffer), length); // Draw the box
}

bool WaterLevelComparer(const block_t *&a, const block_t *&b)
{
    return (a->meta & 0x7) < (b->meta & 0x7);
}

void RecalcSectionWater(chunk_t *chunk, int section)
{
    static std::vector<std::vector<vec3i>> water_levels(9);
    int y_off = (section << 4);
    int x_off = (chunk->x << 4);
    int z_off = (chunk->z << 4);

    for (size_t i = 0; i < water_levels.size(); i++)
        water_levels[i].clear();

    for (int _x = 0; _x < 16; _x++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _y = 0; _y < 16; _y++)
            {
                vec3i pos = vec3i{_x + x_off, _y + y_off, _z + z_off};
                block_t *block = get_block_at(pos);
                if (block)
                {
                    BlockID block_id = block->get_blockid();
                    if (is_fluid(block_id))
                        water_levels[get_fluid_meta_level(block)].push_back(pos);
                }
            }
        }
    }
    for (size_t l = 0; l < water_levels.size(); l++)
    {
        std::vector<vec3i> &levels = water_levels[water_levels.size() - 1 - l];
        for (size_t b = 0; b < levels.size(); b++)
        {
            vec3i pos = levels[b];
            block_t *block = get_block_at(pos);
            update_fluid(block, pos);
        }
    }
}
bool DrawScene(Mtx view, bool transparency)
{

    guVector chunkPos;
    guVector relative = guVector();
    relative.x = 0;
    relative.y = 0;
    relative.z = 0;
    relative = AnglesToVector(-xrot, yrot, 1);
    chunk_t *chunks = get_chunks();

    vec3i rc_block;
    vec3i rc_block_face;
    bool draw_block_outline = false;
    bool facing_block = false;
    if (raycast(vec3d{-pos.x - .5, pos.y - .5, -pos.z - .5}, vec3d{relative.x, relative.y, relative.z}, 4, &rc_block, &rc_block_face))
    {
        draw_block_outline = facing_block = true;
    }
    if (facing_block)
    {
        BlockID target_blockid = destroy_block ? BlockID::air : BlockID::leaves;
        if (destroy_block || place_block)
        {
            vec3i editable_pos = destroy_block ? (rc_block) : (rc_block + rc_block_face);
            block_t *editable_block = get_block_at(editable_pos);
            if (place_block)
                update_block_at(editable_pos);
            for (int i = 0; i < 6; i++)
                update_block_at(editable_pos + face_offsets[i]);
            if (editable_block)
            {
                editable_block->set_blockid(target_blockid);
                editable_block->set_visibility(target_blockid != BlockID::air && target_blockid != BlockID::flowing_water && target_blockid != BlockID::water);
                editable_block->meta = 0;
                update_block_at(rc_block);
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
    place_block = false;
    destroy_block = false;
    relative.x = 0;
    relative.y = 0;
    relative.z = 0;
    if (transparency)
        light_engine_update();
    for (int i = 0; i < CHUNK_COUNT; i++)
    {
        chunk_t *chunk = &chunks[i];
        if (chunk)
        {
            for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
            {
                chunkvbo_t &vbo = chunk->vbos[j];
                vbo.visible = false;
                chunkPos.x = -(chunk->x << 4);
                chunkPos.y = (j << 4) + 16;
                chunkPos.z = -(chunk->z << 4);
                if (fabs(chunkPos.x - pos.x - 8) > 56 || fabs(chunkPos.y - pos.y - 8) > 56 || fabs(chunkPos.z - pos.z - 8) > 56)
                {
                    // Free vbo if the section is out of range.
                    chunk->vbos[j].solid_buffer = nullptr;
                    chunk->vbos[j].solid_buffer_length = 0;
                    chunk->vbos[j].transparent_buffer = nullptr;
                    chunk->vbos[j].transparent_buffer_length = 0;
                    chunk->vbos[j].dirty = true;
                    continue;
                }
                if (fabs(chunkPos.x - pos.x - 8) > 40 || fabs(chunkPos.y - pos.y - 8) > 40 || fabs(chunkPos.z - pos.z - 8) > 40)
                    continue;
                if (transparency && frameCounter % 12 == 0)
                {
                    RecalcSectionWater(chunk, j);
                }
                // Visibility check - don't render things behind the camera
                /*
                if (relative.x >= 0.5 && chunkPos.x + 16 > pos.x)
                    continue;
                if (relative.x <= -0.5 && chunkPos.x - 16 < pos.x)
                    continue;
                if (relative.y >= 0.5 && chunkPos.y + 16 > pos.y)
                    continue;
                if (relative.y <= -0.5 && chunkPos.y - 16 < pos.y)
                    continue;
                if (relative.z >= 0.5 && chunkPos.y + 16 > pos.z)
                    continue;
                if (relative.z <= -0.5 && chunkPos.y - 16 < pos.z)
                    continue;
                */
                vbo.visible = true;
                if (vbo.dirty && vbo.visible && !light_engine_busy())
                {
                    vbo.dirty = false;
                    chunk->recalculate_section(j);
                    chunk->build_vbo(j, false);
                    chunk->build_vbo(j, true);
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
        }
    }
    for (int i = 0; i < CHUNK_COUNT; i++)
    {
        chunk_t *chunk = &chunks[i];
        if (chunk)
        {
            for (int j = 0; j < VERTICAL_SECTION_COUNT; j++)
            {
                chunkvbo_t &vbo = chunk->vbos[j];
                if (!vbo.visible)
                    continue;
                chunkPos.x = (chunk->x << 4);
                chunkPos.y = (j << 4);
                chunkPos.z = (chunk->z << 4);
                if (!transparency)
                {
                    if (vbo.cached_solid_buffer && vbo.cached_solid_buffer_length)
                    {
                        Render(view, chunkPos, vbo.cached_solid_buffer, vbo.cached_solid_buffer_length);
                    }
                }
                else
                {
                    if (vbo.cached_transparent_buffer && vbo.cached_transparent_buffer_length)
                    {
                        Render(view, chunkPos, vbo.cached_transparent_buffer, vbo.cached_transparent_buffer_length);
                    }
                }
            }
        }
    }
    if (transparency && outline && draw_block_outline)
    {
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        guVector outlinePos = guVector();
        outlinePos.x = +rc_block.x - .5;
        outlinePos.y = +rc_block.y - .5;
        outlinePos.z = +rc_block.z - .5;
        Render(view, outlinePos, outline, outline_len);
        Transform(view, outlinePos);

        vtx_is_drawing = true;
        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Vertex(vertex_property_t{vec3f{-2, 1.1f, -2}, 0, 0, 0}, 1.0f);
        GX_Vertex(vertex_property_t{vec3f{2, 1.1f, -2}, 128, 0, 0}, 1.0f);
        GX_Vertex(vertex_property_t{vec3f{2, 1.1f, 2}, 128, 128, 0}, 1.0f);
        GX_Vertex(vertex_property_t{vec3f{-2, 1.1f, 2}, 0, 128, 0}, 1.0f);
        GX_End();
        vtx_is_drawing = false;
    }
    return true;
}
void PrepareTexture(GXTexObj texture)
{
    // setup texture coordinate generation
    // args: texcoord slot 0-7, matrix type, source to generate texture coordinates from, matrix to use
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    // Set up TEV to paint the textures properly.

    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    // Load up the textures (just one this time).
    GX_LoadTexObj(&texture, GX_TEXMAP0);
}
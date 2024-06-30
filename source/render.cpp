#include "render.hpp"

#include "white_tpl.h"
#include "clouds_tpl.h"
#include "sun_tpl.h"
#include "moon_tpl.h"
#include "blockmap_tpl.h"
#include "particles_tpl.h"

const GXColor sky_color = {0x88, 0xBB, 0xFF, 0xFF};

GXTexObj white_texture;
GXTexObj clouds_texture;
GXTexObj sun_texture;
GXTexObj moon_texture;
GXTexObj blockmap_texture;
GXTexObj particles_texture;

// Animated textures
water_texanim_t water_still_anim;
lava_texanim_t lava_still_anim;

void init_texture(GXTexObj &texture, const void *data_src, uint32_t data_len)
{
    TPLFile tpl_file;
    TPL_OpenTPLFromMemory(&tpl_file, (void *)data_src, data_len);
    TPL_GetTexture(&tpl_file, 0, &texture);
    GX_InitTexObjFilterMode(&texture, GX_NEAR, GX_NEAR);
    GX_InitTexObjWrapMode(&texture, GX_MIRROR, GX_MIRROR);
    TPL_CloseTPLFile(&tpl_file);
}

void init_textures()
{

    // Basic textures
    init_texture(white_texture, white_tpl, white_tpl_size);
    init_texture(clouds_texture, clouds_tpl, clouds_tpl_size);
    init_texture(sun_texture, sun_tpl, sun_tpl_size);
    init_texture(moon_texture, moon_tpl, moon_tpl_size);
    init_texture(particles_texture, particles_tpl, particles_tpl_size);
    init_texture(blockmap_texture, blockmap_tpl, blockmap_tpl_size);

    // Animated textures

    // Lava
    lava_still_anim.target = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&blockmap_texture));
    lava_still_anim.dst_width = 256;
    lava_still_anim.tile_width = 16;
    lava_still_anim.tile_height = 16;
    lava_still_anim.dst_x = 208;
    lava_still_anim.dst_y = 224;
    lava_still_anim.flow_dst_x = 224;
    lava_still_anim.flow_dst_y = 224;

    // Water
    water_still_anim.target = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&blockmap_texture));
    water_still_anim.dst_width = 256;
    water_still_anim.tile_width = 16;
    water_still_anim.tile_height = 16;
    water_still_anim.dst_x = 208;
    water_still_anim.dst_y = 192;
    water_still_anim.flow_dst_x = 224;
    water_still_anim.flow_dst_y = 192;
}

void update_textures()
{
    water_still_anim.update();
    lava_still_anim.update();

    static void *texture_buf = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&blockmap_texture));
    static uint32_t texture_buflen = GX_GetTexBufferSize(GX_GetTexObjWidth(&blockmap_texture), GX_GetTexObjHeight(&blockmap_texture), GX_GetTexObjFmt(&blockmap_texture), GX_FALSE, GX_FALSE);
    DCFlushRange(texture_buf, texture_buflen);
    GX_InvalidateTexAll();
}

void use_texture(GXTexObj &texture)
{
    GX_LoadTexObj(&texture, GX_TEXMAP0);
}

void init_fog(Mtx44 &projection_mtx, uint16_t viewport_width)
{
    static bool fog_init_done = false;

    static GXFogAdjTbl fog_adjust_table;

    if (fog_init_done)
        return;
    GX_InitFogAdjTable(&fog_adjust_table, viewport_width, projection_mtx);
    GX_SetFogRangeAdj(GX_ENABLE, viewport_width >> 1, &fog_adjust_table);
    fog_init_done = true;
}

void use_fog(bool use, view_t view, GXColor color, float start, float end)
{
    GX_SetFog(use ? GX_FOG_PERSP_LIN : GX_FOG_NONE, start, end, view.near, view.far, color);
}

void use_ortho(view_t view)
{
    // Prepare projection matrix for rendering GUI elements
    Mtx44 ortho_mtx;
    guOrtho(ortho_mtx, 0, view.height, 0, view.width, 0, view.far);
    GX_LoadProjectionMtx(ortho_mtx, GX_ORTHOGRAPHIC);

    // Prepare position matrix for rendering GUI elements
    Mtx flat_matrix;
    guMtxIdentity(flat_matrix);
    guMtxTransApply(flat_matrix, flat_matrix, 0.0F, 0.0F, -0.5F);
    GX_LoadPosMtxImm(flat_matrix, GX_PNMTX0);
}

void use_perspective(view_t view)
{
    // Prepare projection matrix for rendering the world
    Mtx44 prespective_mtx;
    guPerspective(prespective_mtx, view.fov, view.aspect, view.near, view.far);
    GX_LoadProjectionMtx(prespective_mtx, GX_PERSPECTIVE);

    // Init fog params
    init_fog(prespective_mtx, uint16_t(view.width));
}

static Mtx view_mtx;
Mtx &get_view_matrix()
{
    static bool view_mtx_init = false;
    if (!view_mtx_init)
    {
        // Setup our view matrix at the origin looking down the -Z axis with +Y up
        guVector cam = {0.0F, 0.0F, 0.0F},
                 up = {0.0F, 1.0F, 0.0F},
                 look = {0.0F, 0.0F, -1.0F};
        guLookAt(view_mtx, &cam, &up, &look);
        view_mtx_init = true;
    }
    return view_mtx;
}

uint8_t face_brightness_values[] = {
    153, 153, 127, 255, 204, 204};
inline uint8_t get_face_brightness(uint8_t face)
{
    return face_brightness_values[face];
}
extern uint8_t light_map[256];
inline uint8_t get_face_light_index(vec3i pos, uint8_t face, chunk_t *near, block_t *default_block = nullptr)
{
    vec3i other = pos + face_offsets[face];
    block_t *other_block = get_block_at(other, near);
    if (!other_block)
    {
        if (default_block)
            return default_block->light;
        return 255;
    }
    return other_block->light;
}

int render_face(vec3i pos, uint8_t face, uint32_t texture_index, chunk_t *near, block_t *block)
{
    if (!base3d_is_drawing || face >= 6)
    {
        // This is just a dummy call to GX_Vertex to keep buffer size up to date.
        for (int i = 0; i < 4; i++)
            GX_VertexLit(vertex_property_t(), 0, 0);
        return 4;
    }
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    if (!block)
        block = get_block_at(pos, near);
    uint8_t lighting = get_face_light_index(pos, face, near, block);
    switch (face)
    {
    case 0:
        GX_VertexLit({vertex_pos + vec3f{-.5, -.5, -.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, 0.5, -.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, 0.5, 0.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, -.5, 0.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 1:
        GX_VertexLit({vertex_pos + vec3f{0.5, -.5, 0.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, 0.5, 0.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, 0.5, -.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, -.5, -.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 2:
        GX_VertexLit({vertex_pos + vec3f{-.5, -.5, -.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, -.5, 0.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, -.5, 0.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, -.5, -.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 3:
        GX_VertexLit({vertex_pos + vec3f{0.5, 0.5, -.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, 0.5, 0.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, 0.5, 0.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, 0.5, -.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 4:
        GX_VertexLit({vertex_pos + vec3f{0.5, -.5, -.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, 0.5, -.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, 0.5, -.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, -.5, -.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 5:
        GX_VertexLit({vertex_pos + vec3f{-.5, -.5, 0.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5, 0.5, 0.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, 0.5, 0.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5, -.5, 0.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    default:
        break;
    }
    return 4;
}

guVector angles_to_vector(float x, float y, float distance, guVector vec)
{
    vec.x += cos(DegToRad(x)) * sin(DegToRad(y)) * distance;
    vec.y += -sin(DegToRad(x)) * distance;
    vec.z += cos(DegToRad(x)) * cos(DegToRad(y)) * distance;
    return vec;
}

// Function to calculate the signed distance from a point to a frustum plane
float distance_to_plane(const vec3f &point, const frustum_t &frustum, int planeIndex)
{
    plane_t plane = frustum.planes[planeIndex];
    return plane.direction.x * point.x + plane.direction.y * point.y + plane.direction.z * point.z - plane.distance;
}

// Function to calculate the distance from a point to a frustum
float distance_to_frustum(const vec3f &point, const frustum_t &frustum)
{
    float minDistance = distance_to_plane(point, frustum, 0);

    for (int i = 1; i < 6; ++i)
    {
        float distance = distance_to_plane(point, frustum, i);
        if (distance < minDistance)
        {
            minDistance = distance;
        }
    }

    return minDistance;
}

// Function to calculate the frustum planes from camera parameters
frustum_t calculate_frustum(camera_t &camera)
{
    frustum_t frustum;

    // Calculate half-width and half-height at near plane
    float half_fov = camera.fov * 0.5f;

    // Calculate forward vector
    guVector forward = angles_to_vector(camera.rot.x, camera.rot.y, -1);
    guVector backward = vec3f() - forward;

    // Calculate the 4 perspective frustum planes (not near and far)
    guVector right_vec = angles_to_vector(camera.rot.x, camera.rot.y + 90 + half_fov, 1);
    guVector left_vec = angles_to_vector(camera.rot.x, camera.rot.y - 90 - half_fov, 1);
    guVector up_vec = angles_to_vector(camera.rot.x + 90 + half_fov, camera.rot.y, 1);
    guVector down_vec = angles_to_vector(camera.rot.x - 90 - half_fov, camera.rot.y, 1);

    // Calculate points on the near and far planes
    guVector near_center = camera.position + (vec3f(forward) * (camera.near));
    guVector far_center = camera.position + (vec3f(forward) * (camera.far));

    // Normalize the directions
    guVecNormalize(&forward);
    guVecNormalize(&backward);
    guVecNormalize(&right_vec);
    guVecNormalize(&left_vec);
    guVecNormalize(&up_vec);
    guVecNormalize(&down_vec);

    // Calculate the frustum plane equations in the form Ax + By + Cz + D = 0
    frustum.planes[0].direction = left_vec;
    frustum.planes[0].distance = guVecDotProduct(&left_vec, &near_center);

    frustum.planes[1].direction = right_vec;
    frustum.planes[1].distance = guVecDotProduct(&right_vec, &near_center);

    frustum.planes[2].direction = down_vec;
    frustum.planes[2].distance = guVecDotProduct(&down_vec, &near_center);

    frustum.planes[3].direction = up_vec;
    frustum.planes[3].distance = guVecDotProduct(&up_vec, &near_center);

    frustum.planes[4].direction = forward;
    frustum.planes[4].distance = guVecDotProduct(&forward, &near_center);

    frustum.planes[5].direction = backward;
    frustum.planes[5].distance = guVecDotProduct(&backward, &far_center);

    return frustum;
}

void transform_view(Mtx view, guVector chunkPos)
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
    guMtxTrans(offset, -chunkPos.x, -chunkPos.y, -chunkPos.z);
    guMtxTrans(posmtx, player_pos.x, player_pos.y, player_pos.z);

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

void transform_view_screen(Mtx view, guVector off)
{
    Mtx model, modelview;
    Mtx posmtx;

    // Reset matrices
    guMtxIdentity(model);
    guMtxIdentity(posmtx);

    // Position the object on the screen
    guMtxTrans(posmtx, -off.x, -off.y, -off.z);
    // Apply matrices
    guMtxConcat(posmtx, model, model);
    guMtxInverse(model, model);
    guMtxConcat(model, view, modelview);
    GX_LoadPosMtxImm(modelview, GX_PNMTX0);
}

void draw_particle(camera_t &camera, vec3f pos, uint32_t texture_index, float size, uint8_t brightness)
{
    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    GX_BeginGroup(GX_QUADS, 4);
    for (int i = 0; i < 4; i++)
    {
        int x = (i == 0 || i == 3);
        int y = (i > 1);
        guVector vertex{(x - 0.5f) * size, (y - 0.5f) * size, 0};

        Mtx44 rot_mtx;

        // Rotate the vertex
        guMtxRotDeg(rot_mtx, 'x', camera.rot.x);
        guVecMultiply(rot_mtx, &vertex, &vertex);
        guMtxRotDeg(rot_mtx, 'y', camera.rot.y);
        guVecMultiply(rot_mtx, &vertex, &vertex);

        // Translate the vertex
        vertex.x += pos.x;
        vertex.y += pos.y;
        vertex.z += pos.z;

        GX_VertexLit(vertex_property_t(vertex, TEXTURE_X(texture_index) + x * 4, TEXTURE_Y(texture_index) + y * 4), brightness);
    }
    GX_EndGroup();
}

void draw_particles(camera_t &camera, particle_t *particles, int count)
{

    // Bake vertex properties
    Mtx44 rot_mtx;
    vertex_property_t vertices[4];
    for (int i = 0; i < 4; i++)
    {
        int x = (i == 0 || i == 3);
        int y = (i > 1);
        guVector vertex{(x - 0.5f), (y - 0.5f), 0};

        guMtxRotDeg(rot_mtx, 'x', camera.rot.x);
        guVecMultiply(rot_mtx, &vertex, &vertex);
        guMtxRotDeg(rot_mtx, 'y', camera.rot.y);
        guVecMultiply(rot_mtx, &vertex, &vertex);

        vertices[i] = vertex_property_t(vertex, x << 2, y << 2);
    }

    // Draw particles on a per-type basis
    for (int t = 0; t < PTYPE_MAX; t++)
    {
        // Enumerate visible particles
        int visible_count = 0;
        for (int i = 0; i < count; i++)
        {
            particle_t &particle = particles[i];
            if (particle.life_time && particle.type == t)
                visible_count++;
        }

        if (visible_count == 0)
            continue;

        if (t == PTYPE_BLOCK_BREAK)
        {
            // Enable indexed colors
            GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
        }
        else if (t == PTYPE_TINY_SMOKE)
        {
            // Enable direct colors
            GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        }

        use_texture(t == 0 ? blockmap_texture : particles_texture);

        GX_BeginGroup(GX_QUADS, visible_count << 2);

        // Draw particles
        for (int i = 0; i < count; i++)
        {
            particle_t &particle = particles[i];
            if (particle.life_time && particle.type == t)
            {
                for (int j = 0; j < 4; j++)
                {
                    vertex_property_t vertex = vertices[j];
                    vertex.pos = vertex.pos * particle.size * (1.f / 64.f);
                    vertex.pos = vertex.pos + particle.position;

                    if (t == PTYPE_BLOCK_BREAK)
                    {
                        vertex.x_uv += particle.u;
                        vertex.y_uv += particle.v;
                        GX_VertexLit(vertex, particle.brightness);
                    }
                    else if (t == PTYPE_TINY_SMOKE)
                    {
                        int x = (j == 0 || j == 3);
                        int y = (j > 1);
                        vertex.x_uv = (x << 4) + (int(particle.life_time * 16.0f / float(particle.max_life_time)) << 4);
                        vertex.y_uv = (y << 4);
                        vertex.color_r = particle.r;
                        vertex.color_g = particle.g;
                        vertex.color_b = particle.b;
                        vertex.color_a = particle.a;
                        GX_Vertex(vertex);
                    }
                }
            }
        }

        GX_EndGroup();
    }
}

void draw_stars()
{
    static bool generated = false;
    static vec3f vertices[780 << 2];
    if (!generated)
    {
        JavaLCGInit(10842);
        int index = 0;
        for (int var3 = 0; var3 < 1500; ++var3)
        {
            double var4 = (double)(JavaLCGFloat() * 2.0F - 1.0F);
            double var6 = (double)(JavaLCGFloat() * 2.0F - 1.0F);
            double var8 = (double)(JavaLCGFloat() * 2.0F - 1.0F);
            double var10 = (double)(0.25F + JavaLCGFloat() * 0.25F);
            double var12 = var4 * var4 + var6 * var6 + var8 * var8;
            if (var12 < 1.0 && var12 > 0.01)
            {
                var12 = 1.0 / std::sqrt(var12);
                var4 *= var12;
                var6 *= var12;
                var8 *= var12;
                double var14 = var4 * 100.0;
                double var16 = var6 * 100.0;
                double var18 = var8 * 100.0;
                double var20 = std::atan2(var4, var8);
                double var22 = std::sin(var20);
                double var24 = std::cos(var20);
                double var26 = std::atan2(std::sqrt(var4 * var4 + var8 * var8), var6);
                double var28 = std::sin(var26);
                double var30 = std::cos(var26);
                double var32 = JavaLCGDouble() * M_TWOPI;
                double var34 = std::sin(var32);
                double var36 = std::cos(var32);

                for (int var38 = 0; var38 < 4; ++var38)
                {
                    double var39 = 0.0;
                    double var41 = (double)((var38 & 2) - 1) * var10;
                    double var43 = (double)(((var38 + 1) & 2) - 1) * var10;
                    double var47 = var41 * var36 - var43 * var34;
                    double var49 = var43 * var36 + var41 * var34;
                    double var53 = var47 * var28 + var39 * var30;
                    double var55 = var39 * var28 - var47 * var30;
                    double var57 = var55 * var22 - var49 * var24;
                    double var61 = var49 * var22 + var55 * var24;
                    vertices[index++] = vec3f(var14 + var57, var16 + var53, var18 + var61);
                }
            }
        }
        generated = true;
    }
    int brightness_level = (get_star_brightness() * 255);
    if (brightness_level > 0)
    {
        GX_BeginGroup(GX_QUADS, 780 << 2);
        for (int i = 0; i < (780 << 2); i++)
            GX_Vertex(vertex_property_t(vertices[i], 0, 0, brightness_level, brightness_level, brightness_level, brightness_level));
        GX_EndGroup();
    }
}

float get_celestial_angle()
{
    int daytime_ticks = (int)(tickCounter % 24000L);
    float normalized_daytime = ((float)daytime_ticks + partialTicks) / 24000.0F - 0.25F;
    if (normalized_daytime < 0.0F)
    {
        ++normalized_daytime;
    }

    if (normalized_daytime > 1.0F)
    {
        --normalized_daytime;
    }

    float var4 = normalized_daytime;
    normalized_daytime = 1.0F - (float)((std::cos((double)normalized_daytime * M_PI) + 1.0) / 2.0);
    normalized_daytime = var4 + (normalized_daytime - var4) / 3.0F;
    return normalized_daytime;
}
float get_star_brightness()
{
    float var2 = get_celestial_angle();
    float var3 = 1.0F - (std::cos(var2 * M_TWOPI) * 2.0F + 0.75F);
    if (var3 < 0.0F)
    {
        var3 = 0.0F;
    }

    if (var3 > 1.0F)
    {
        var3 = 1.0F;
    }

    return var3 * var3 * 0.5F;
}
float get_sky_multiplier()
{
    float var2 = get_celestial_angle();
    float var3 = std::cos(var2 * M_TWOPI) * 2.0F + 0.5F;
    if (var3 < 0.0F)
    {
        var3 = 0.0F;
    }

    if (var3 > 1.0F)
    {
        var3 = 1.0F;
    }
    return var3;
}
GXColor get_sky_color(bool cave_darkness)
{
    float elevation_brightness = std::pow(std::clamp((player_pos.y) * 0.03125f, 0.0f, 1.0f), 2.0f);
    float sky_multiplier = get_sky_multiplier();
    float brightness = elevation_brightness * sky_multiplier;
    return GXColor{uint8_t(sky_color.r * brightness), uint8_t(sky_color.g * brightness), uint8_t(sky_color.b * brightness), 0xFF};
}
void draw_sky(GXColor background)
{
    // The normals of the sky elements are inverted. Fix this by disabling backface culling
    GX_SetCullMode(GX_CULL_NONE);

    // Disable fog
    GX_SetFog(GX_FOG_NONE, RENDER_DISTANCE * 0.67f * 16 - 16, RENDER_DISTANCE * 0.67f * 16 - 8, CAMERA_NEAR, CAMERA_FAR, background);

    // Use additive blending
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);

    use_texture(white_texture);

    Mtx celestial_rotated_view;
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    guVector axis{1, 0, 0};
    guMtxRotAxisDeg(celestial_rotated_view, &axis, get_celestial_angle() * 360.0f);
    guMtxConcat(get_view_matrix(), celestial_rotated_view, celestial_rotated_view);

    transform_view(celestial_rotated_view, player_pos);
    draw_stars();
    float size = 30.0f;
    float dist = 98.0f;

    // Draw sun
    use_texture(sun_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(vec3f(-size, +dist, -size), 0, 0));
    GX_Vertex(vertex_property_t(vec3f(+size, +dist, -size), BASE3D_UV_FRAC, 0));
    GX_Vertex(vertex_property_t(vec3f(+size, +dist, +size), BASE3D_UV_FRAC, BASE3D_UV_FRAC));
    GX_Vertex(vertex_property_t(vec3f(-size, +dist, +size), 0, BASE3D_UV_FRAC));
    GX_EndGroup();

    // Draw moon
    use_texture(moon_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(vec3f(-size, -dist, +size), 0, BASE3D_UV_FRAC));
    GX_Vertex(vertex_property_t(vec3f(+size, -dist, +size), BASE3D_UV_FRAC, BASE3D_UV_FRAC));
    GX_Vertex(vertex_property_t(vec3f(+size, -dist, -size), BASE3D_UV_FRAC, 0));
    GX_Vertex(vertex_property_t(vec3f(-size, -dist, -size), 0, 0));
    GX_EndGroup();

    // Enable fog but place it further away.
    GX_SetFog(GX_FOG_PERSP_LIN, RENDER_DISTANCE * 2 * 16 - 16, RENDER_DISTANCE * 3 * 16 - 16, CAMERA_NEAR, CAMERA_FAR, background);

    // Use default blend mode
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);

    // Here we use 0 fractional bits for the position data, because we're drawing large objects.
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    constexpr float scale = 1.0f / BASE3D_POS_FRAC;
    // Clouds texture is massive.
    size = 2048.0f * scale;

    // The clouds are placed at y=108
    dist = 108.0f * scale;

    // Reset transform and move the clouds
    transform_view(get_view_matrix(), guVector{0, 0, ((tickCounter % 40960) + partialTicks) * 0.05f});

    // Clouds are a bit yellowish white. They are also affected by the time
    float sky_multiplier = get_sky_multiplier();
    uint8_t sky_r = (sky_multiplier * 229.5f + 25.5f);
    uint8_t sky_g = (sky_multiplier * 229.5f + 25.5f);
    uint8_t sky_b = (sky_multiplier * 216.75f + 38.25f);

    // Draw clouds
    use_texture(clouds_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(vec3f(-size, dist, +size), 0, BASE3D_UV_FRAC << 1, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(+size, dist, +size), BASE3D_UV_FRAC << 1, BASE3D_UV_FRAC << 1, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(+size, dist, -size), BASE3D_UV_FRAC << 1, 0, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(-size, dist, -size), 0, 0, sky_r, sky_g, sky_b));
    GX_EndGroup();
}

#include "render.hpp"

#include "white_tpl.h"
#include "clouds_tpl.h"
#include "sun_tpl.h"
#include "moon_tpl.h"
#include "blockmap_tpl.h"
#include "water_still_tpl.h"

const GXColor sky_color = {0x88, 0xBB, 0xFF, 0xFF};

GXTexObj white_texture;
GXTexObj clouds_texture;
GXTexObj sun_texture;
GXTexObj moon_texture;
GXTexObj blockmap_texture;
GXTexObj water_still_texture;

void init_texture(GXTexObj &texture, const void *data_src, uint32_t data_len)
{
    TPLFile tpl_file;
    TPL_OpenTPLFromMemory(&tpl_file, (void *)data_src, data_len);
    TPL_GetTexture(&tpl_file, 0, &texture);
    GX_InitTexObjFilterMode(&texture, GX_NEAR, GX_NEAR);
    TPL_CloseTPLFile(&tpl_file);
}

void init_textures()
{
    init_texture(white_texture, white_tpl, white_tpl_size);
    init_texture(clouds_texture, clouds_tpl, clouds_tpl_size);
    init_texture(sun_texture, sun_tpl, sun_tpl_size);
    init_texture(moon_texture, moon_tpl, moon_tpl_size);
    init_texture(water_still_texture, water_still_tpl, water_still_tpl_size);
    init_texture(blockmap_texture, blockmap_tpl, blockmap_tpl_size);
}

void use_texture(GXTexObj &texture)
{
    GX_LoadTexObj(&texture, GX_TEXMAP0);
}

void extract_texanim_info(texanim_t &anim, GXTexObj &src_texture, GXTexObj &dst_texture)
{
    anim.dst_width = GX_GetTexObjWidth(&dst_texture);
    anim.src_width = GX_GetTexObjWidth(&src_texture);
    anim.src_height = GX_GetTexObjHeight(&src_texture);
    anim.source = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&src_texture));
    anim.target = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&dst_texture));
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
    guOrtho(ortho_mtx, 0, view.height, 0, view.width, 0, CAMERA_FAR);
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

uint8_t get_face_brightness(uint8_t face)
{
    switch (face)
    {
    case 0:
    case 1:
        return 153;
    case 2:
        return 127;
    case 4:
    case 5:
        return 204;
    default:
        return 255;
    }
}
extern uint8_t light_map[256];
float get_face_light_index(vec3i pos, uint8_t face)
{
    vec3i other = pos + face_offsets[face];
    block_t *other_block = get_block_at(other);
    if (!other_block)
        return get_block_at(pos)->light;
    return other_block->light;
}

int render_face(vec3i pos, uint8_t face, uint32_t texture_index)
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
    uint8_t lighting = get_face_light_index(pos, face);
    switch (face)
    {
    case 0:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 1:
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 2:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 3:
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 4:
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 5:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
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
    const float *plane = frustum.planes[planeIndex];
    return plane[0] * point.x + plane[1] * point.y + plane[2] * point.z + plane[3];
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
// Function to normalize a 3D vector
void normalize(float &x, float &y, float &z)
{
    float length = std::sqrt(x * x + y * y + z * z);
    if (length != 0.0f)
    {
        float invLength = 1.0f / length;
        x *= invLength;
        y *= invLength;
        z *= invLength;
    }
}

void cross_product(float *a, float *b, float *out)
{
    guVector v_a = guVector{a[0], a[1], a[2]};
    guVector v_b = guVector{b[0], b[1], b[2]};
    guVector v_out;
    guVecCross(&v_a, &v_b, &v_out);
    out[0] = v_out.x;
    out[1] = v_out.y;
    out[2] = v_out.z;
}
float dot_product(float *a, float *b)
{
    guVector v_a = guVector{a[0], a[1], a[2]};
    guVector v_b = guVector{b[0], b[1], b[2]};
    return guVecDotProduct(&v_a, &v_b);
}
// Function to calculate the frustum planes from camera parameters
frustum_t calculate_frustum(camera_t &camera)
{
    frustum_t frustum;

    // Calculate the camera's right and up vectors
    float right[3], up[3];
    float global_up[3] = {0.0f, 1.0f, 0.0f};
    cross_product(camera.forward, global_up, right);
    cross_product(right, camera.forward, up);

    // Calculate points on the near and far planes
    float nearCenter[3] = {
        camera.position[0] + camera.forward[0] * camera.near,
        camera.position[1] + camera.forward[1] * camera.near,
        camera.position[2] + camera.forward[2] * camera.near};

    float farCenter[3] = {
        camera.position[0] + camera.forward[0] * camera.far,
        camera.position[1] + camera.forward[1] * camera.far,
        camera.position[2] + camera.forward[2] * camera.far};

    // Calculate the normals to the frustum planes
    float normalLeft[3] = {-right[0], -right[1], -right[2]};
    float normalRight[3] = {right[0], right[1], right[2]};
    float normalBottom[3] = {-up[0], -up[1], -up[2]};
    float normalTop[3] = {up[0], up[1], up[2]};
    float normalNear[3] = {-camera.forward[0], -camera.forward[1], -camera.forward[2]};
    float normalFar[3] = {camera.forward[0], camera.forward[1], camera.forward[2]};

    // Normalize the normals
    normalize(normalLeft[0], normalLeft[1], normalLeft[2]);
    normalize(normalRight[0], normalRight[1], normalRight[2]);
    normalize(normalBottom[0], normalBottom[1], normalBottom[2]);
    normalize(normalTop[0], normalTop[1], normalTop[2]);
    normalize(normalNear[0], normalNear[1], normalNear[2]);
    normalize(normalFar[0], normalFar[1], normalFar[2]);

    // Calculate the frustum plane equations in the form Ax + By + Cz + D = 0
    frustum.planes[0][0] = normalLeft[0];
    frustum.planes[0][1] = normalLeft[1];
    frustum.planes[0][2] = normalLeft[2];
    frustum.planes[0][3] = dot_product(normalLeft, nearCenter);

    frustum.planes[1][0] = normalRight[0];
    frustum.planes[1][1] = normalRight[1];
    frustum.planes[1][2] = normalRight[2];
    frustum.planes[1][3] = dot_product(normalRight, nearCenter);

    frustum.planes[2][0] = normalBottom[0];
    frustum.planes[2][1] = normalBottom[1];
    frustum.planes[2][2] = normalBottom[2];
    frustum.planes[2][3] = dot_product(normalBottom, nearCenter);

    frustum.planes[3][0] = normalTop[0];
    frustum.planes[3][1] = normalTop[1];
    frustum.planes[3][2] = normalTop[2];
    frustum.planes[3][3] = dot_product(normalTop, nearCenter);

    frustum.planes[4][0] = normalNear[0];
    frustum.planes[4][1] = normalNear[1];
    frustum.planes[4][2] = normalNear[2];
    frustum.planes[4][3] = dot_product(normalNear, nearCenter);

    frustum.planes[5][0] = normalFar[0];
    frustum.planes[5][1] = normalFar[1];
    frustum.planes[5][2] = normalFar[2];
    frustum.planes[5][3] = dot_product(normalFar, farCenter);

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
    GX_SetFog(GX_FOG_NONE, RENDER_DISTANCE * 0.67f * 16 - 16, RENDER_DISTANCE * 0.67f * 16 - 8, 0.1F, 3000.0F, background);

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
    GX_Vertex(vertex_property_t(vec3f(-size, +dist, -size), 0x000, 0x000));
    GX_Vertex(vertex_property_t(vec3f(+size, +dist, -size), 0x100, 0x000));
    GX_Vertex(vertex_property_t(vec3f(+size, +dist, +size), 0x100, 0x100));
    GX_Vertex(vertex_property_t(vec3f(-size, +dist, +size), 0x000, 0x100));
    GX_EndGroup();

    // Draw moon
    use_texture(moon_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(vec3f(-size, -dist, +size), 0x000, 0x100));
    GX_Vertex(vertex_property_t(vec3f(+size, -dist, +size), 0x100, 0x100));
    GX_Vertex(vertex_property_t(vec3f(+size, -dist, -size), 0x100, 0x000));
    GX_Vertex(vertex_property_t(vec3f(-size, -dist, -size), 0x000, 0x000));
    GX_EndGroup();

    // Enable fog but place it further away.
    GX_SetFog(GX_FOG_PERSP_LIN, RENDER_DISTANCE * 2 * 16 - 16, RENDER_DISTANCE * 3 * 16 - 16, 0.1F, 3000.0F, background);

    // Use default blend mode
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GX_SetColorUpdate(GX_TRUE);

    // Here we use 0 fractional bits for the position data, because we're drawing large objects.
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);

    // Clouds texture is massive.
    size = 2048.0f / BASE3D_POS_FRAC;

    // The clouds are placed at y=108
    dist = 108.0f / BASE3D_POS_FRAC;

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
    GX_Vertex(vertex_property_t(vec3f(-size, dist, +size), 0x000, 0x200, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(+size, dist, +size), 0x200, 0x200, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(+size, dist, -size), 0x200, 0x000, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(-size, dist, -size), 0x000, 0x000, sky_r, sky_g, sky_b));
    GX_EndGroup();
}

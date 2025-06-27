#ifndef _RENDER_HPP_
#define _RENDER_HPP_

#include <math/vec3i.hpp>
#include <math/vec3f.hpp>
#include <stack>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <ogc/gx.h>
#include <ogc/tpl.h>
#include <ogcsys.h>

#include "chunk.hpp"
#include "texturedefs.h"
#include "base3d.hpp"
#include "texanim.hpp"
#include "particle.hpp"
#include "gertex/gertex.hpp"

extern guVector player_pos;
extern float xrot, yrot;
extern uint8_t light_map[1024];

struct plane_t
{
    vec3f direction;
    vfloat_t distance;
};

struct frustum_t
{
    plane_t planes[6]; // Six planes of the frustum
};

struct camera_t
{
    vec3f rot;       // Camera rotation (in degrees)
    vec3f position;  // Camera position
    vfloat_t fov;    // Field of view (in degrees)
    vfloat_t aspect; // Aspect ratio
    vfloat_t near;   // Near clipping plane
    vfloat_t far;    // Far clipping plane
};

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

void init_textures();

void update_textures();

void use_texture(GXTexObj &texture);

int render_face(vec3i pos, uint8_t face, uint32_t texture_index, chunk_t *near = nullptr, block_t *block = nullptr, uint8_t min_y = 0, uint8_t max_y = 16);

void render_single_block(block_t &selected_block, bool transparency);

void render_single_block_at(block_t &selected_block, vec3i pos, bool transparency);

void render_single_item(uint32_t texture_index, bool transparency, uint8_t light = 0xFF);

void render_item_pixel(uint32_t texture_index, uint8_t x, uint8_t y, bool x_next, bool y_next, uint8_t light);

vec3f angles_to_vector(float x, float y);

vec3f vector_to_angles(const vec3f &vec);

float distance_to_plane(const vec3f &point, const frustum_t &frustum, int planeIndex);

float distance_to_frustum(const vec3f &point, const frustum_t &frustum);

frustum_t calculate_frustum(camera_t &camera);

void transform_view(gertex::GXMatrix view, guVector world_pos, guVector object_scale = guVector{1, 1, 1}, guVector object_rot = guVector{0, 0, 0}, bool load = true);

void transform_view_screen(gertex::GXMatrix view, guVector screen_pos, guVector object_scale = guVector{1, 1, 1}, guVector object_rot = guVector{0, 0, 0}, bool load = true);

void draw_sky(GXColor background);

GXColor get_sky_color(bool cave_darkness = true);

GXColor get_lightmap_color(uint8_t light);

void draw_particle(camera_t &camera, vec3f pos, uint32_t texture_index, float size, uint8_t brightness);

void draw_particles(camera_t &camera, particle *particles, int count);

void draw_stars();

float get_sky_multiplier();

float get_star_brightness();
#endif
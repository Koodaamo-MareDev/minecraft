#ifndef _RENDER_HPP_
#define _RENDER_HPP_
#include "vec3i.hpp"
#include "vec3f.hpp"

#include "ported/JavaRandom.hpp"
#include "chunk_new.hpp"
#include "texturedefs.h"
#include "base3d.hpp"
#include "texanim.hpp"
#include "base3d.hpp"
#include "particle.hpp"

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <ogc/gx.h>
#include <ogc/tpl.h>
#include <ogcsys.h>

#define CAMERA_WIDTH (640.0f)
#define CAMERA_HEIGHT (480.0f)
#define CAMERA_NEAR (0.1f)
#define CAMERA_FAR (300.0f)

extern guVector player_pos;
extern float xrot, yrot;
extern int tickCounter;
extern float partialTicks;

struct plane_t
{
    vec3f direction;
    float distance;
};

struct frustum_t
{
    plane_t planes[6]; // Six planes of the frustum
};

struct camera_t
{
    vec3f rot;
    vec3f position; // Camera position
    float fov;      // Field of view angle
    float aspect;   // Aspect ratio
    float near;     // Near clipping plane
    float far;      // Far clipping plane
};

struct view_t
{
    float width = CAMERA_WIDTH;
    float height = CAMERA_HEIGHT;
    float fov = 90;
    float near = CAMERA_NEAR;
    float far = CAMERA_FAR;
    float aspect = CAMERA_WIDTH / CAMERA_HEIGHT;
    float yscale = 1.2f;
    bool widescreen = false;
    view_t(float width, float height, bool widescreen, float fov, float near, float far, float yscale)
    {

        // Aspect ratio correction
        this->yscale = yscale;
        this->aspect = width / height;
        this->width = width;
        this->height = height;
        this->fov = fov;
        this->near = near;
        this->far = far;
        this->widescreen = widescreen;
    }
};

void init_texture(GXTexObj &texture, void *data_src, uint32_t data_len);

void init_textures();

void update_textures();

void use_texture(GXTexObj &texture);

void init_fog(Mtx44 &projection_mtx, uint16_t viewport_width);

void use_fog(bool use, view_t view, GXColor color, float start, float end);

void use_ortho(view_t view);

void use_perspective(view_t view);

Mtx &get_view_matrix();

int render_face(vec3i pos, uint8_t face, uint32_t texture_index, chunk_t *near = nullptr, block_t *block = nullptr);

guVector angles_to_vector(float x, float y, float distance, guVector vec = guVector());

float distance_to_plane(const vec3f &point, const frustum_t &frustum, int planeIndex);

float distance_to_frustum(const vec3f &point, const frustum_t &frustum);

frustum_t calculate_frustum(camera_t &camera);

void transform_view(Mtx view, guVector chunkPos);

void transform_view_screen(Mtx view, guVector off);

void draw_sky(GXColor background);

GXColor get_sky_color(bool cave_darkness = true);

void draw_particle(camera_t &camera, vec3f pos, uint32_t texture_index, float size, uint8_t brightness);

void draw_particles(camera_t &camera, particle_t *particles, int count);

void draw_stars();

float get_sky_multiplier();

float get_star_brightness();
#endif
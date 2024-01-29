#ifndef _RENDER_HPP_
#define _RENDER_HPP_
#include "vec3i.hpp"
#include "vec3f.hpp"

#include "ported/JavaRandom.hpp"
#include "chunk_new.hpp"
#include "texturedefs.h"
#include "base3d.hpp"
#include "texanim.hpp"
#include <ogcsys.h>

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <ogc/gx.h>
#include <ogc/tpl.h>

extern guVector player_pos;
extern float xrot, yrot;
extern int tickCounter;
extern float partialTicks;

struct frustum_t
{
    float planes[6][4]; // Six planes of the frustum
};

struct camera_t
{
    float position[3]; // Camera position
    float forward[3];  // Camera forward vector
    float fov;         // Field of view angle
    float aspect;      // Aspect ratio
    float near;        // Near clipping plane
    float far;         // Far clipping plane
};

void init_texture(GXTexObj &texture, void *data_src, uint32_t data_len);

void init_textures();

void use_texture(GXTexObj &texture);

void extract_texanim_info(texanim_t &anim, GXTexObj &src_texture, GXTexObj &dst_texture);

float get_face_light_index(vec3i pos, uint8_t face);
uint8_t get_face_brightness(uint8_t face);
int render_face(vec3i pos, uint8_t face, uint32_t texture_index);

guVector angles_to_vector(float x, float y, float distance, guVector vec = guVector());

float distance_to_plane(const vec3f &point, const frustum_t &frustum, int planeIndex);

float distance_to_frustum(const vec3f &point, const frustum_t &frustum);

void normalize(float &x, float &y, float &z);

void cross_product(float *a, float *b, float *out);

float dot_product(float *a, float *b);

frustum_t calculate_frustum(camera_t &camera);

void transform_view(Mtx view, guVector chunkPos);

void draw_sky(Mtx view, GXColor background);

GXColor get_sky_color(bool cave_darkness = true);

void draw_stars();

float get_sky_multiplier();

float get_star_brightness();
#endif
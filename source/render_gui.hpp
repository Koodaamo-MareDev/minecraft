#ifndef _RENDER_GUI_HPP_
#define _RENDER_GUI_HPP_

#include <cstdint>
#include <ogc/gx.h>

#include <math/vec3i.hpp>
#include "texturedefs.h"
#include "render.hpp"
#include "base3d.hpp"
#include "ported/Random.hpp"
#include <string>

constexpr float GUI_DEFAULT_SCALE = 1.0f / BASE3D_POS_FRAC;

GXColor get_text_color(char c);

GXColor get_text_color_at(int index);

int draw_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, vfloat_t u1, vfloat_t v1, vfloat_t u2, vfloat_t v2, float scale = GUI_DEFAULT_SCALE);

int draw_colored_quad(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float scale = GUI_DEFAULT_SCALE);

int draw_colored_sprite(GXTexObj &texture, Vec2i pos, Vec2i size, vfloat_t u1, vfloat_t v1, vfloat_t u2, vfloat_t v2, GXColor color = GXColor{0xFF, 0xFF, 0xFF, 0xFF}, float scale = GUI_DEFAULT_SCALE);

int draw_colored_sprite_3d(GXTexObj &texture, Vec3f center, Vec3f size, Vec3f offset, Vec3f right, Vec3f up, vfloat_t u1, vfloat_t v1, vfloat_t u2, vfloat_t v2, GXColor color);

vfloat_t text_width_3d(std::string str);

void draw_text_3d(Vec3f pos, std::string str, GXColor color);

int fill_screen_texture(GXTexObj &texture, gertex::GXView &view, vfloat_t u1, vfloat_t v1, vfloat_t u2, vfloat_t v2, float scale = GUI_DEFAULT_SCALE);

int draw_simple_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, float scale = GUI_DEFAULT_SCALE);

uint8_t obfuscate_char(javaport::Random &rng, uint8_t original);

#endif
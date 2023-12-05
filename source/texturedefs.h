#ifndef TEXTUREDEFS_H
#define TEXTUREDEFS_H

#define OPTIMIZE_UVS

#ifdef OPTIMIZE_UVS
#define TexCoord GX_TexCoord2u8
#define UV_SCALE (16)
#define TEXTURE_X(x) (UV_SCALE * (x & 15))
#define TEXTURE_Y(y) (UV_SCALE * ((y >> 4) & 15))

#define TEXTURE_NX(x) (TEXTURE_X(x))
#define TEXTURE_NY(y) (TEXTURE_Y(y))
#define TEXTURE_PX(x) (TEXTURE_X(x) + UV_SCALE)
#define TEXTURE_PY(y) (TEXTURE_Y(y) + UV_SCALE)

#define TEXTURE_COUNT (UV_SCALE << 4)
#define VERTEX_ATTR_LENGTH (3 * sizeof(int16_t) + 1 * sizeof(uint8_t) + 1 * sizeof(uint8_t) + 2 * sizeof(uint16_t))
#define VERTEX_ATTR_LENGTH_DIRECTCOLOR (3 * sizeof(int16_t) + 4 * sizeof(uint8_t) + 1 * sizeof(uint8_t) + 2 * sizeof(uint16_t))
#else
#define TEXTURES_PER_UNIT 16
#define UV_POSTSCALE 1.0f
#define TEXTURE_X(x) (x % 16)
#define TEXTURE_Y(y) (y / 16)
#define TexCoord GX_TexCoord2f32
#define UV_SCALE UV_POSTSCALE / TEXTURES_PER_UNIT
#define VERTEX_ATTR_LENGTH 23
#endif

#define WATER_SIDE (205)
#define LAVA_SIDE (239)

#endif
#ifndef TEXTUREDEFS_H
#define TEXTUREDEFS_H

#define OPTIMIZE_UVS

#ifdef OPTIMIZE_UVS
#define TEXTURE_X(x) (x & 15)
#define TEXTURE_Y(y) ((y >> 4) & 15)
#define TexCoord GX_TexCoord2u8
#define UV_SCALE (uint8_t(8))
#define TEXTURE_COUNT (UV_SCALE << 4)
#define VERTEX_ATTR_LENGTH 17
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
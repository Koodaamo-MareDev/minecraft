#ifndef _TEXANIM_HPP_
#define _TEXANIM_HPP_
#include <cstdio>
#include <malloc.h>
#include <cstring>
#include <ogc/gx.h>

#define TA_DST (1)
#define TA_SRC (0)

struct texanim_t
{
    uint32_t src_format;
    uint32_t dst_format;
    void* source = nullptr;
    void* target = nullptr;
    uint32_t src_width = 16;
    uint32_t src_height = 512;
    uint32_t dst_width = 256;
    uint32_t tile_width = 16;
    uint32_t tile_height = 16;
    uint32_t dst_x = 0;
    uint32_t dst_y = 0;
    uint32_t current_y = 0;
    void update();
};


void SimpleTexObjInit(GXTexObj *texture, int width, int height, void *buffer = nullptr);
#endif
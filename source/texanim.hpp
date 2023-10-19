#ifndef _TEXANIM_HPP_
#define _TEXANIM_HPP_
#include <cstdio>
#include <malloc.h>
#include <cstring>
#include <ogc/gx.h>

struct texanim_t
{
    uint32_t* source = nullptr;
    uint32_t* target = nullptr;
    uint16_t source_width = 16;
    uint16_t source_height = 512;
    uint16_t target_width = 256;
    uint16_t copy_width = 16;
    uint16_t copy_height = 16;
    uint16_t copy_x = 0;
    uint16_t copy_y = 0;
    uint8_t current_frame = 0;
    uint8_t total_frames = 1;
    void update();
};


void SimpleTexObjInit(GXTexObj *texture, int width, int height, void *buffer = nullptr);
#endif
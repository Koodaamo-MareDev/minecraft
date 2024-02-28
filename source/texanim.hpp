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

    void update()
    {
        this->current_y += tile_height;
        this->current_y %= src_height;
        void *src_ptr = (void *)(u32(this->source) + (this->current_y * src_width * 4));
        for (uint32_t y = dst_y; y < dst_y + tile_height; y += 4)
        {
            for (uint32_t x = dst_x; x < dst_x + tile_width; x += 4)
            {
                void* dst_ptr = (void *)(u32(target) + (dst_width * 4 * y + x * 16));
                memcpy(dst_ptr, src_ptr, 64);
                src_ptr = (void *)(u32(src_ptr) + 64);
            }
        }
    }

};

inline void SimpleTexObjInit(GXTexObj *texture, int width, int height, void *buffer = nullptr)
{
    unsigned int buffer_len = GX_GetTexBufferSize(width, height, GX_TF_RGBA8, GX_FALSE, 0);
    if (!buffer)
        buffer = memalign(32, buffer_len);
    printf("Texture Info: Width: %d, Height: %d, Pixel Size: %d, Raw Size: %d", width, height, buffer_len, width * height * 4);
    GX_InitTexObj(texture, buffer, width, height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
}
#endif
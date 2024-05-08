#ifndef _TEXANIM_HPP_
#define _TEXANIM_HPP_
#include <cstdio>
#include <malloc.h>
#include <cstring>
#include <ogc/gx.h>

#define TA_DST (1)
#define TA_SRC (0)

class texanim_t
{
public:
    void *source = nullptr;
    void *target = nullptr;
    void *src_ptr = nullptr;
    uint32_t dst_width = 256;
    uint32_t tile_width = 16;
    uint32_t tile_height = 16;
    uint32_t dst_x = 0;
    uint32_t dst_y = 0;

    virtual void update();
    void copy(void *src_ptr, uint32_t dst_x, uint32_t dst_y);
    void copy_tpl();
};

class water_texanim_t : public texanim_t
{
    float data_a[256] = {0};
    float data_b[256] = {0};
    float data_c[256] = {0};
    float data_d[256] = {0};
    uint8_t texture_data_still[256 * 4] = {0};
    uint8_t texture_data_flow[256 * 4] = {0};
    uint32_t frame = 0;

public:
    uint32_t flow_dst_x = 0;
    uint32_t flow_dst_y = 0;
    void update() override;
};

class lava_texanim_t : public texanim_t
{
    float data_a[256] = {0};
    float data_b[256] = {0};
    float data_c[256] = {0};
    float data_d[256] = {0};
    uint8_t texture_data_still[256 * 4] = {0};
    uint8_t texture_data_flow[256 * 4] = {0};
    uint32_t frame = 0;

public:
    uint32_t flow_dst_x = 0;
    uint32_t flow_dst_y = 0;
    void update() override;
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
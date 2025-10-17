#ifndef _TEXANIM_HPP_
#define _TEXANIM_HPP_
#include <cstdio>
#include <malloc.h>
#include <cstring>
#include <ogc/gx.h>

#define TA_DST (1)
#define TA_SRC (0)

class TexAnim
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

class WaterTexAnim : public TexAnim
{
    float data_a[256] = {0};
    float data_b[256] = {0};
    float data_c[256] = {0};
    float data_d[256] = {0};

    float data_e[256] = {0};
    float data_f[256] = {0};
    float data_g[256] = {0};
    float data_h[256] = {0};
    uint8_t texture_data_still[256 * 4] = {0};
    uint8_t texture_data_flow[256 * 4] = {0};
    uint32_t frame = 0;

public:
    uint32_t flow_dst_x = 0;
    uint32_t flow_dst_y = 0;
    void update() override;
};

class LavaTexanim : public TexAnim
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
#endif
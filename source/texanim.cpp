#include "texanim.hpp"

void SimpleTexObjInit(GXTexObj *texture, int width, int height, void *buffer)
{
    unsigned int buffer_len = GX_GetTexBufferSize(width, height, GX_TF_RGBA8, GX_FALSE, 0);
    if (!buffer)
        buffer = memalign(32, buffer_len);
    printf("Texture Info: Width: %d, Height: %d, Pixel Size: %d, Raw Size: %d", width, height, buffer_len, width * height * 4);
    GX_InitTexObj(texture, buffer, width, height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
}

void texanim_t::update()
{
    total_frames = this->source_height / this->source_width;
    this->current_frame++;
    this->current_frame %= this->total_frames;
    uint32_t current_y = uint32_t(this->current_frame) * copy_height; 
    for (uint16_t src_y = 0; src_y < this->copy_height; src_y++)
    {
        uint32_t* src_ptr = this->source + ((current_y + src_y) * uint32_t(this->source_width) << 2);
        uint32_t* dst_ptr = this->target + (uint32_t(this->copy_x) + ((uint32_t(this->copy_y + src_y) * uint32_t(this->target_width)) << 2));
        memcpy(dst_ptr, src_ptr, copy_width << 2);
    }
}

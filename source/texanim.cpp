#include "texanim.hpp"
#include <cmath>
void texanim_t::update()
{
    if (src_ptr)
    {
        copy(src_ptr, dst_x, dst_y);
    }
}

void texanim_t::copy(void *src_ptr, uint32_t dst_x, uint32_t dst_y)
{
    for (uint32_t y = dst_y; y < dst_y + tile_height; y += 4)
    {
        for (uint32_t x = dst_x; x < dst_x + tile_width; x += 4)
        {
            void *dst_ptr = (void *)(u32(target) + (dst_width * 4 * y + x * 16));
            memcpy(dst_ptr, src_ptr, 64);
            src_ptr = (void *)(u32(src_ptr) + 64);
        }
    }
}

void texanim_t::copy_tpl()
{
    const int tpl_size = tile_width * tile_height << 2;
    uint8_t tmp[tpl_size];
    uint8_t *tmp_ptr = tmp;
    uint8_t *src_ptr = (uint8_t *)this->src_ptr;
    memcpy(tmp, src_ptr, tpl_size);
    for (uint32_t y = 0; y < tile_height; y++)
    {
        for (uint32_t x = 0; x < tile_width; x++)
        {

            // Get the index to the 4x4 texel in the target texture
            int index = (tile_width << 2) * (y & ~3) + ((x & ~3) << 4);

            // Put the data within the 4x4 texel into the target texture
            int index_within = ((x & 3) + ((y & 3) << 2)) << 1;

            src_ptr[index + index_within + 1] = *tmp_ptr++;
            src_ptr[index + index_within + 32] = *tmp_ptr++;
            src_ptr[index + index_within + 33] = *tmp_ptr++;
            src_ptr[index + index_within] = *tmp_ptr++;
        }
    }
}

void water_texanim_t::update()
{
    static bool flip = false;
    float *data_a = flip ? this->data_a : this->data_b;
    float *data_b = flip ? this->data_b : this->data_a;

    // Update the still texture
    for (int x = 0; x < 16; ++x)
    {
        for (int y = 0; y < 16; ++y)
        {
            float val = 0.0f;

            for (int i = x - 1; i <= x + 1; ++i)
            {
                int tex_x = i & 0xF;
                int tex_y = y & 0xF;
                val += data_a[tex_x + tex_y * 16];
            }

            data_b[x + y * 16] = val / 3.3f + (data_c[x + y * 16]) * 0.8f;
        }
    }

    for (int x = 0; x < 16; ++x)
    {
        for (int y = 0; y < 16; ++y)
        {
            data_c[x + y * 16] += data_d[x + y * 16] * 0.05f;
            if (data_c[x + y * 16] < 0.0f)
            {
                data_c[x + y * 16] = 0.0f;
            }
            data_d[x + y * 16] -= 0.1f;
            if (rand() % 20 == 0)
            {
                data_d[x + y * 16] = 0.5f;
            }
        }
    }

    uint8_t *texture_data_ptr = (uint8_t *)texture_data_still;
    for (int i = 0; i < 256; ++i)
    {
        float val = data_b[i];
        if (val > 1.0f)
        {
            val = 1.0f;
        }
        if (val < 0.0f)
        {
            val = 0.0f;
        }
        val *= val;
        uint8_t r = (uint8_t)(val * 32.0f + 32.0f);
        uint8_t g = (uint8_t)(val * 64.0f + 50.0f);
        uint8_t b = 0xFF;
        uint8_t a = (uint8_t)(val * 50.0f + 146.0f);

        *((uint8_t *)(texture_data_ptr++)) = r;
        *((uint8_t *)(texture_data_ptr++)) = g;
        *((uint8_t *)(texture_data_ptr++)) = b;
        *((uint8_t *)(texture_data_ptr++)) = a;
    }


    // Update the flow texture
    for (int x = 0; x < 16; ++x)
    {
        for (int y = 0; y < 16; ++y)
        {
            float val = 0.0f;

            for (int i = y - 2; i <= y; ++i)
            {
                int tex_x = x & 0xF;
                int tex_y = i & 0xF;
                val += data_a[tex_x + tex_y * 16];
            }

            data_b[x + y * 16] = val / 3.2f + (data_c[x + y * 16]) * 0.8f;
        }
    }

    for (int x = 0; x < 16; ++x)
    {
        for (int y = 0; y < 16; ++y)
        {
            data_c[x + y * 16] += data_d[x + y * 16] * 0.05f;
            if (data_c[x + y * 16] < 0.0f)
            {
                data_c[x + y * 16] = 0.0f;
            }
            data_d[x + y * 16] -= 0.3f;
            if (rand() % 5 == 0)
            {
                data_d[x + y * 16] = 0.5f;
            }
        }
    }

    texture_data_ptr = (uint8_t *)texture_data_flow;
    for (int i = 0; i < 256; ++i)
    {
        float val = data_b[(i - frame * 16) & 0xFF];
        if (val > 1.0f)
        {
            val = 1.0f;
        }
        if (val < 0.0f)
        {
            val = 0.0f;
        }
        val *= val;
        uint8_t r = (uint8_t)(val * 32.0f + 32.0f);
        uint8_t g = (uint8_t)(val * 64.0f + 50.0f);
        uint8_t b = 0xFF;
        uint8_t a = (uint8_t)(val * 50.0f + 146.0f);

        *((uint8_t *)(texture_data_ptr++)) = r;
        *((uint8_t *)(texture_data_ptr++)) = g;
        *((uint8_t *)(texture_data_ptr++)) = b;
        *((uint8_t *)(texture_data_ptr++)) = a;
    }

    // Swap the data arrays on the next frame
    flip = !flip;

    this->src_ptr = this->texture_data_still;
    texanim_t::copy_tpl();
    texanim_t::update();

    this->src_ptr = this->texture_data_flow;
    texanim_t::copy_tpl();
    for (int x = 0; x < 32; x += 16)
    {
        for (int y = 0; y < 32; y += 16)
        {
            texanim_t::copy(this->texture_data_flow, flow_dst_x + x, flow_dst_y + y);
        }
    }
    frame--;
}

void lava_texanim_t::update()
{
    static bool flip = false;
    float *data_a = flip ? this->data_a : this->data_b;
    float *data_b = flip ? this->data_b : this->data_a;
    for (int x = 0; x < 16; ++x)
    {
        for (int y = 0; y < 16; ++y)
        {
            float val = 0.0f;
            int sin_x = (int)(std::sin((float)x * (float)M_TWOPI / 16.0F) * 1.2F);
            int sin_y = (int)(std::sin((float)y * (float)M_TWOPI / 16.0F) * 1.2F);
            for (int i = x - 1; i <= x + 1; ++i)
            {
                for (int j = y - 1; j <= y + 1; ++j)
                {
                    int tex_x = (i + sin_x) & 0xF;
                    int tex_y = (j + sin_y) & 0xF;
                    val += data_a[tex_x + tex_y * 16];
                }
            }
            data_b[x + y * 16] = val / 10.0F + (data_c[(x & 15) + (y & 15) * 16] + data_c[((x + 1) & 15) + (y & 15) * 16] + data_c[((x + 1) & 15) + ((y + 1) & 15) * 16] + data_c[(x & 15) + ((y + 1) & 15) * 16]) / 4.0F * 0.8F;
            data_c[x + y * 16] += data_d[x + y * 16] * 0.01f;
            if (data_c[x + y * 16] < 0.0f)
            {
                data_c[x + y * 16] = 0.0f;
            }
            data_d[x + y * 16] -= 0.06f;
            if (rand() % 200 == 0)
            {
                data_d[x + y * 16] = 1.5f;
            }
        }
    }

    uint8_t *texture_data_ptr = (uint8_t *)texture_data_still;
    for (int i = 0; i < 256; ++i)
    {
        float val = data_b[i] * 2.0f;
        if (val > 1.0f)
        {
            val = 1.0f;
        }
        if (val < 0.0f)
        {
            val = 0.0f;
        }

        uint8_t r = (uint8_t)(val * 100.0f + 155.0f);
        uint8_t g = (uint8_t)(val * val * 255.0f);
        uint8_t b = (uint8_t)(val * val * val * val * 128.0f);

        *((uint8_t *)(texture_data_ptr++)) = r;
        *((uint8_t *)(texture_data_ptr++)) = g;
        *((uint8_t *)(texture_data_ptr++)) = b;
        *((uint8_t *)(texture_data_ptr++)) = 0xFF;
    }

    // Swap the data arrays on the next frame
    flip = !flip;

    // Update the flow texture
    int offset = ((frame++ / 3) * 16) & 0xFF;
    memcpy(texture_data_flow, texture_data_still + offset * 4, (256 - offset) * 4);
    memcpy(texture_data_flow + (256 - offset) * 4, texture_data_still, offset * 4);

    this->src_ptr = this->texture_data_still;
    texanim_t::copy_tpl();
    texanim_t::update();

    this->src_ptr = this->texture_data_flow;
    texanim_t::copy_tpl();
    for (int x = 0; x < 32; x += 16)
    {
        for (int y = 0; y < 32; y += 16)
        {
            texanim_t::copy(this->texture_data_flow, flow_dst_x + x, flow_dst_y + y);
        }
    }
}
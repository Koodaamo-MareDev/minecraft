#include "base3d.hpp"

bool base3d_is_drawing;
uint16_t __group_vtxcount;

static uint8_t face_colors[6 * 4 * 4]; // 6 faces, 4 ambient occlusion values, 4 color components each
void init_face_normals()
{
    float multipliers[] = {0.8f, 0.8f, 0.5f, 1.0f, 0.6f, 0.6f};
    uint8_t *elem = face_colors;
    for (int i = 0; i < 24; i++)
    {
        uint8_t face = (multipliers[i % 6] * (1.f - (0.21f * int(i / 6)))) * 255;
        *elem++ = face;
        *elem++ = face;
        *elem++ = face;
        *elem++ = 255;
    }
    GX_SetArray(GX_VA_CLR1, face_colors, 4 * sizeof(uint8_t));
}
#include "native_gx.h"

void nat_gx_position3i(Frame *frame, char *type)
{
    Value v;
    assert(strcmp(type, "(III)V") == 0);
    v = frame_stackpop(frame);
    int16_t position_z = v.i;
    v = frame_stackpop(frame);
    int16_t position_y = v.i;
    v = frame_stackpop(frame);
    int16_t position_x = v.i;
    GX_Position3s16(position_x, position_y, position_z);
}

void nat_gx_position3f(Frame *frame, char *type)
{
    Value v;
    assert(strcmp(type, "(FFF)V") == 0);
    v = frame_stackpop(frame);
    float position_z = v.f;
    v = frame_stackpop(frame);
    float position_y = v.f;
    v = frame_stackpop(frame);
    float position_x = v.f;
    GX_Position3f32(position_x, position_y, position_z);
}

void nat_gx_normal1x(Frame *frame, char *type)
{
    Value v;
    assert(strcmp(type, "(I)V") == 0);
    v = frame_stackpop(frame);
    uint8_t index = (v.i & 0xFF);
    GX_Normal1x8(index);
}

void nat_gx_color4i(Frame *frame, char *type)
{
    Value v;
    assert(strcmp(type, "(IIII)V") == 0);
    v = frame_stackpop(frame);
    uint8_t a = (v.i & 0xFF);
    v = frame_stackpop(frame);
    uint8_t b = (v.i & 0xFF);
    v = frame_stackpop(frame);
    uint8_t g = (v.i & 0xFF);
    v = frame_stackpop(frame);
    uint8_t r = (v.i & 0xFF);
    GX_Color4u8(r, g, b, a);
}

void nat_gx_texcoord2i(Frame *frame, char *type)
{
    Value v;
    assert(strcmp(type, "(II)V") == 0);
    v = frame_stackpop(frame);
    uint16_t y = (v.i & 0xFF);
    v = frame_stackpop(frame);
    uint16_t x = (v.i & 0xFF);
    GX_TexCoord2u16(x, y);
}
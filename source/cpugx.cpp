#include <ogc/gx.h>
#include "cpugx.hpp"
#include <cstring>
#include <malloc.h>
#include <cstdio>
int current_size = 0;
void *buffer_start = nullptr;
void *current_buffer = nullptr;

void _writeFloat(float f)
{
    *(float *)current_buffer = f;
    current_buffer = (void *)((char *)current_buffer + 4);
};

void _writeInt(int i)
{
    *(int *)current_buffer = i;
    current_buffer = (void *)((char *)current_buffer + 4);
};

void _writeUInt(unsigned int i)
{
    *(unsigned int *)current_buffer = i;
    current_buffer = (void *)((char *)current_buffer + 4);
};

void _writeChar(char c)
{
    *(char *)current_buffer = c;
    current_buffer = (void *)((char *)current_buffer + 1);
};

void _writeUChar(unsigned char c)
{
    *(unsigned char *)current_buffer = c;
    current_buffer = (void *)((char *)current_buffer + 1);
};

void _writeShort(short s)
{
    *(short *)current_buffer = s;
    current_buffer = (void *)((char *)current_buffer + 2);
};

void _writeUShort(unsigned short s)
{
    *(unsigned short *)current_buffer = s;
    current_buffer = (void *)((char *)current_buffer + 2);
};

void CPUGX_BeginDispList(void *buffer, int size)
{
    current_size = size;
    buffer_start = current_buffer = buffer;
}

void CPUGX_Begin(u8 primitve, u8 vtxfmt, u16 vtxcnt)
{
    u8 reg = primitve | (vtxfmt & 7);
    _writeUChar(reg);
    _writeUShort(vtxcnt);
}
void CPUGX_Position3f32(f32 x, f32 y, f32 z)
{
    _writeFloat(x);
    _writeFloat(y);
    _writeFloat(z);
}
void CPUGX_Color3f32(f32 r, f32 g, f32 b)
{
    _writeUChar((u8)(r * 255.0));
    _writeUChar((u8)(g * 255.0));
    _writeUChar((u8)(b * 255.0));
}
void CPUGX_Color3u8(u8 r, u8 g, u8 b)
{
    _writeUChar(r);
    _writeUChar(g);
    _writeUChar(b);
}
void CPUGX_TexCoord2u8(u8 s, u8 t)
{
    _writeUChar(s);
    _writeUChar(t);
}
void CPUGX_End()
{
    return;
}
int CPUGX_EndDispList()
{ 
    u32 diff = ((char *)current_buffer - (char *)buffer_start);
    memset(current_buffer, 0, current_size - diff);
    return diff;
}

void* CPUGX_memalign(u64 align, u64 amount) {
    printf("Memalign %lld bytes", align + amount);
    return malloc(align + amount);
}
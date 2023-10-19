#ifndef _CPUGX_HPP_
#define _CPUGX_HPP_
#include <ogc/gx.h>
#define CPUGX_QUADS GX_QUADS
#define CPUGX_QUADS GX_QUADS
#define CPUGX_VTXFMT0 GX_VTXFMT0
#define ALIGNPTR get_aligned_pointer_32
void _writeFloat(float f);

void _writeInt(int i);

void _writeUInt(unsigned int i);

void _writeChar(char c);

void _writeUChar(unsigned char c);

void _writeShort(short c);

void _writeUShort(unsigned short c);

void CPUGX_BeginDispList(void *buffer, int size);

void CPUGX_Begin(u8 primitve, u8 vtxfmt, u16 vtxcnt);

void CPUGX_Position3f32(f32 x, f32 y, f32 z);

void CPUGX_Color3f32(f32 r, f32 g, f32 b);

void CPUGX_Color3u8(u8 r, u8 g, u8 b);

void CPUGX_TexCoord2u8(u8 s, u8 t);

void CPUGX_End();

int CPUGX_EndDispList();

void *CPUGX_memalign(u64 align, u64 amount);
#endif
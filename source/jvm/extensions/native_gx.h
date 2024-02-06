#ifndef _NATIVE_GX_H_
#define _NATIVE_GX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <ogc/gx.h>

#include "../class.h"
#include "../memory.h"
#include "../native.h"


void nat_gx_position3i(Frame *frame, char *type);
void nat_gx_position3f(Frame *frame, char *type);
void nat_gx_normal1x(Frame *frame, char *type);
void nat_gx_color4i(Frame *frame, char *type);
void nat_gx_texcoord2i(Frame *frame, char *type);

#ifdef __cplusplus
}
#endif

#endif
#ifndef _JVM_NATIVE_H_
#define _JVM_NATIVE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "class.h"
#include "memory.h"

typedef enum JavaClass {
	NONE_CLASS     = -1,
	LANG_SYSTEM    = 0,
	LANG_STRING    = 1,
	IO_PRINTSTREAM = 2,
	GRAPHICS_GX    = 3
} JavaClass;

JavaClass native_javaclass(char *classname);
void *native_javaobj(JavaClass jclass, char *objname, char *objtype);
int native_javamethod(Frame *frame, JavaClass jclass, char *name, char *type);

#ifdef __cplusplus
}
#endif

#endif
#ifndef _JVM_FILE_H_
#define _JVM_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "class.h"

void file_free(ClassFile *class);
int file_read(FILE *fp, ClassFile *class);
char *file_errstr(int i);

#ifdef __cplusplus
}
#endif

#endif
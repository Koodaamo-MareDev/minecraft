#ifndef DEBUGLOG_HPP
#define DEBUGLOG_HPP

#include <cstdio>
#include <cstdarg>
#include <cstring>

#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/conf.h>
#include <ogc/gx.h>
#include <ogc/consol.h>

namespace debug
{
    void init(GXRModeObj *mode);
    void print(const char *fmt, ...);
    void copy_to(void *fb);
    void deinit();

} // namespace debug

#endif // DEBUGLOG_HPP
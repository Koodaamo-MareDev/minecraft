#ifndef SYSTEM_TIME_HPP
#define SYSTEM_TIME_HPP
#include <ogc/system.h>
#include <ogc/lwp_watchdog.h>

namespace javaport
{
    class System
    {
    public:
        static int64_t currentTimeMillis()
        {
            return ticks_to_millisecs(SYS_Time());
        }
    };
}

#endif
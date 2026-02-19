#ifndef SYSTEM_TIME_HPP
#define SYSTEM_TIME_HPP
#include <ogc/system.h>
#include <ogc/lwp_watchdog.h>

namespace javaport
{
    class System
    {
    public:
        // Converts Wii time to Unix timestamp and returns it in milliseconds.
        static int64_t currentTimeMillis()
        {
            return ticks_to_millisecs(SYS_Time()) + 946684800000ULL;
        }

        // NOTE: This doesn't reflect the absolute time. Should be only used for calculating elapsed time.
        static int64_t nanoTime()
        {
            return ticks_to_nanosecs(SYS_Time());
        }
    };
}

#endif
#ifndef SYSTEM_TIME_HPP
#define SYSTEM_TIME_HPP

#include "../timers.hpp"

namespace javaport
{
    class System
    {
    public:
        static int64_t currentTimeMillis()
        {
            return time_get();
        }
    };
}

#endif
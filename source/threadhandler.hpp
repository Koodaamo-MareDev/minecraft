#ifndef _THREADHANDLER_HPP_
#define _THREADHANDLER_HPP_

#ifdef USE_THREAD_QUEUE

#include <ogc/lwp.h>
extern lwpq_t __thread_queue;

inline void threadhandler_init()
{
    LWP_InitQueue(&__thread_queue);
}
inline void threadhandler_sleep()
{
    LWP_ThreadBroadcast(__thread_queue);
    LWP_ThreadSleep(__thread_queue);
}
inline void threadhandler_yield()
{
    LWP_YieldThread();
}
inline void threadhandler_broadcast()
{
    LWP_ThreadBroadcast(__thread_queue);
}
#else
#include <sys/unistd.h>

inline void threadhandler_init()
{
}
inline void threadhandler_sleep()
{
    usleep(100);
}
inline void threadhandler_yield()
{
    usleep(1);
}
inline void threadhandler_broadcast()
{
}
#endif
#endif
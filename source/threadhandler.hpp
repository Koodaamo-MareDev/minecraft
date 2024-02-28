#ifndef _THREADHANDLER_HPP_
#define _THREADHANDLER_HPP_
#include <ogc/lwp.h>
extern lwpq_t __thread_queue;

inline void threadqueue_init()
{
    LWP_InitQueue(&__thread_queue);
}
inline void threadqueue_sleep()
{
    LWP_ThreadBroadcast(__thread_queue);
    LWP_ThreadSleep(__thread_queue);
}
inline void threadqueue_yield()
{
    LWP_YieldThread();
}
inline void threadqueue_broadcast()
{
    LWP_ThreadBroadcast(__thread_queue);
}
#endif
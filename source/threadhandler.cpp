#include "threadhandler.hpp"
lwpq_t __thread_queue;

void threadqueue_init()
{
    LWP_InitQueue(&__thread_queue);
}

void threadqueue_sleep()
{
    LWP_ThreadSleep(__thread_queue);
}
void threadqueue_yield()
{
    LWP_YieldThread();
}
void threadqueue_broadcast()
{
    LWP_ThreadBroadcast(__thread_queue);
}
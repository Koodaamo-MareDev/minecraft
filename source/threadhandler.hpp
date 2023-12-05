#ifndef _THREADHANDLER_HPP_
#define _THREADHANDLER_HPP_
#include <ogc/lwp.h>
extern lwpq_t __thread_queue;
void threadqueue_init();
void threadqueue_sleep();
void threadqueue_yield();
void threadqueue_broadcast();
#endif
#ifndef _LOCK_HPP_
#define _LOCK_HPP_

#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <unistd.h>

class lock_t
{
private:
    mutex_t mutex;

    lock_t(const lock_t &);

public:
    void lock()
    {
        while (LWP_MutexTryLock(mutex))
            usleep(1);
    }

    lock_t(mutex_t &mutex) : mutex(mutex)
    {
        if (mutex == LWP_MUTEX_NULL)
        {
            mutex = LWP_MutexInit(&mutex, false);
        }
        lock();
    }

    void unlock()
    {
        LWP_MutexUnlock(mutex);
    }

    ~lock_t()
    {
        unlock();
    }
};

#endif
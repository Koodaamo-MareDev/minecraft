#ifndef _LOCK_HPP_
#define _LOCK_HPP_

#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <unistd.h>

class Lock
{
private:
    mutex_t mutex;

    Lock(const Lock &);

public:
    void lock()
    {
        while (LWP_MutexTryLock(mutex))
            usleep(1);
    }

    Lock(mutex_t &mutex)
    {
        init(mutex);
        this->mutex = mutex;
        lock();
    }

    void unlock()
    {
        LWP_MutexUnlock(mutex);
    }

    ~Lock()
    {
        unlock();
    }
    static void init(mutex_t &mutex)
    {
        if (mutex == LWP_MUTEX_NULL)
        {
            LWP_MutexInit(&mutex, false);
        }
    }
    static void destroy(mutex_t &mutex)
    {
        if (mutex != LWP_MUTEX_NULL)
        {
            LWP_MutexDestroy(mutex);
            mutex = LWP_MUTEX_NULL;
        }
    }
};

#endif
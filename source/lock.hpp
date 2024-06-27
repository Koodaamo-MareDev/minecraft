#ifndef _LOCK_HPP_
#define _LOCK_HPP_

#include <ogc/lwp.h>
#include <ogc/mutex.h>

class lock_t
{
private:
    mutex_t mutex;

    lock_t(const lock_t&);
public:
    void lock()
    {
        while (LWP_MutexLock(mutex))
            LWP_YieldThread();
    }

    lock_t(mutex_t mutex) : mutex(mutex)
    {
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
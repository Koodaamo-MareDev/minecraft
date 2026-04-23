#ifndef WORKER_THREAD_HPP
#define WORKER_THREAD_HPP

#include <gccore.h>
#include <ogc/mutex.h>
#include <ogc/cond.h>
#include <ogc/lwp.h>

// ---------------- Mutex ----------------
class Mutex
{
public:
    Mutex() { LWP_MutexInit(&m_, false); }
    ~Mutex() { LWP_MutexDestroy(m_); }

    void lock() { LWP_MutexLock(m_); }
    void unlock() { LWP_MutexUnlock(m_); }

    mutex_t native() { return m_; }

private:
    mutex_t m_;
};

class LockGuard
{
public:
    explicit LockGuard(Mutex &m) : m_(m) { m_.lock(); }
    ~LockGuard() { m_.unlock(); }

private:
    Mutex &m_;
};

// ---------------- Condition Variable ----------------
class CondVar
{
public:
    CondVar() { LWP_CondInit(&c_); }
    ~CondVar() { LWP_CondDestroy(c_); }

    void wait(Mutex &m) { LWP_CondWait(c_, m.native()); }
    void signal() { LWP_CondSignal(c_); }

private:
    cond_t c_;
};

// ---------------- Worker Thread ----------------
class WorkerThread
{
public:
    using JobFunc = void *(*)(void *);

    WorkerThread()
    {
        LWP_CreateThread(&thread_, threadEntry, this, nullptr, 0, 60);
    }

    ~WorkerThread()
    {
        {
            LockGuard lock(mutex_);
            shutdown_ = true;
            cond_.signal();
        }
        LWP_JoinThread(thread_, nullptr);
    }

    // Submit a job with user data pointer
    void _submit(JobFunc func, void *userData)
    {
        LockGuard lock(mutex_);
        func_ = func;
        data_ = userData;
        has_job_ = true;
        done_ = false;
        cond_.signal();
    }

    template <typename T>
    void submit(void *(*func)(T *), T *data)
    {
        _submit(reinterpret_cast<JobFunc>(func), data);
    }

    // Non-blocking check
    bool is_done()
    {
        LockGuard lock(mutex_);
        return done_;
    }

    // Blocking wait
    void wait()
    {
        LockGuard lock(mutex_);
        while (!done_)
        {
            cond_.wait(mutex_);
        }
    }

private:
    static void *threadEntry(void *arg)
    {
        static_cast<WorkerThread *>(arg)->run();
        return nullptr;
    }

    void run()
    {
        mutex_.lock();

        while (!shutdown_)
        {
            while (!has_job_ && !shutdown_)
            {
                cond_.wait(mutex_);
            }

            if (shutdown_)
                break;

            JobFunc func = func_;
            void *data = data_;

            has_job_ = false;

            mutex_.unlock();

            // ---- Execute job ----
            if (func)
                func(data);

            mutex_.lock();

            done_ = true;
            cond_.signal();
        }

        mutex_.unlock();
    }

private:
    lwp_t thread_;
    Mutex mutex_;
    CondVar cond_;

    JobFunc func_ = nullptr;
    void *data_ = nullptr;

    bool has_job_ = false;
    bool done_ = false;
    bool shutdown_ = false;
};
#endif
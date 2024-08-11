#ifndef _ASYNCLIB_HPP_
#define _ASYNCLIB_HPP_

#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <functional>
#include <vector>
#include <algorithm>
#include <unistd.h>

class async_func
{
private:
    mutex_t mutex;

    // Remove the copy constructor and default constructor to prevent issues with the mutex
    async_func(const async_func &) = delete;
    async_func() = delete;

    std::function<void()> func;

public:
    volatile bool called = false;
    volatile bool awaiting = false;

    void await()
    {
        awaiting = true;
        while (LWP_MutexTryLock(mutex) < 0)
        {
            if (called)
                return;
            usleep(100);
        }
        if (called)
        {
            LWP_MutexUnlock(mutex);
            return;
        }

        called = true;
        func();
        LWP_MutexUnlock(mutex);
    }

    async_func(mutex_t mutex, std::function<void()> func);

    void update()
    {
        if (!LWP_MutexTryLock(mutex))
        {
            if (called)
            {
                LWP_MutexUnlock(mutex);
                return; // The function has already been called in await()
            }

            called = true;
            func();
            LWP_MutexUnlock(mutex);
        }
    }
};

class async_lib
{
private:
    lwp_t thread;

public:
    std::vector<async_func *> funcs;
    volatile bool running = true;
    void add(async_func *func)
    {
        funcs.push_back(func);
    }

    async_lib()
    {
        // Initialize the thread that will run the async functions
        LWP_CreateThread(&thread, async_thread, this, NULL, 0, 50);
    }

    static void *async_thread(void *arg)
    {
        async_lib *lib = (async_lib *)arg;
        while (lib->running)
        {
            // Erase all the async functions that have been called - delete them manually as they are allocated using new
            lib->funcs.erase(std::remove_if(lib->funcs.begin(), lib->funcs.end(), [](async_func *&func)
                                            {
                if (func->called)
                {
                    delete func;
                    return true;
                }
                return false; }),
                             lib->funcs.end());

            for (async_func *&func : lib->funcs)
            {
                if (func->awaiting) // If someone is waiting for this function to finish, we don't want to call it asynchronously anymore
                    continue;
                func->update();
                if (func->called)
                    usleep(100);
            }
            usleep(100);
        }
        return NULL;
    }

    ~async_lib()
    {
        running = false;

        // Delete all the async functions
        funcs.clear();

        // Wait for the thread to finish
        LWP_JoinThread(thread, NULL);
    }

    static void init();
    static void deinit();
};
extern async_lib *asyncLib;

#define WRAP_ASYNC_FUNC(mtx, func) do {static bool __##mtx_run = 1; if (__##mtx_run) { __##mtx_run = 0; new async_func(mtx, [&]() { func; __##mtx_run = 1; }); } } while(0)

#endif
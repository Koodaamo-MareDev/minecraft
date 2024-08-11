#include "asynclib.hpp"

async_lib *asyncLib;

async_func::async_func(mutex_t mutex, std::function<void()> func) : mutex(mutex), func(func)
{
    asyncLib->add(this);
}

void async_lib::init()
{
    asyncLib = new async_lib();
}

void async_lib::deinit()
{
    delete asyncLib;
}

#ifndef CHUNK_MANAGER_HPP
#define CHUNK_MANAGER_HPP

#include <util/worker_thread.hpp>

class World;

class ChunkManager
{
private:
    World *world;
    WorkerThread worker;
    bool thread_active = true;

public:
    void start(World *world);
    void stop();

    bool active();

    void update_loop();
};

#endif
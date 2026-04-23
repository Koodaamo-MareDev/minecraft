#ifndef _LIGHT_HPP_
#define _LIGHT_HPP_

#include <math/vec3i.hpp>
#include <util/worker_thread.hpp>
#include <cstdint>
#include <deque>

class World;
class LightEngine
{
private:
    bool busy_flag = false;
    bool thread_active = true;
    World *world;
    std::deque<Vec3i> pending_updates;
    WorkerThread worker;

    void process(const Vec3i &start);

public:
    LightEngine(World *world = nullptr) : world(world) {}

    void start(World *world = nullptr);
    void stop();
    void restart();

    void post(const Vec3i &location);

    bool busy();

    void update_loop();
};
enum LightType
{
    BLOCK,
    SKY
};

struct LightNode
{
    int x, y, z;
    uint8_t level;
    LightType type;
};

#endif
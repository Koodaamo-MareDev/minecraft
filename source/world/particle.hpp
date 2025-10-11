#ifndef _PARTICLE_HPP_
#define _PARTICLE_HPP_

#define PPHYSIC_FLAG_GRAVITY (1 << 0)
#define PPHYSIC_FLAG_COLLIDE (1 << 1)
#define PPHYSIC_FLAG_FRICTION (1 << 2)

#define PPHYSIC_FLAG_ALL 0xFF

#define PTYPE_BLOCK_BREAK 0
#define PTYPE_TINY_SMOKE 1
#define PTYPE_GENERIC 2

#define PTYPE_MAX 3

#include "entity.hpp"

class Particle : public EntityBase
{
public:
    uint8_t max_life_time;
    uint8_t life_time;
    uint8_t physics;
    uint8_t type;
    uint8_t size;
    uint8_t brightness;

    // The data union is used to store extra information about the particle
    // For example, the color of the particle or the UV coordinates of the texture
    // Alternatively, the data union can be used to store a pointer to a custom data structure
    // The behavior of the data union is determined by the type of the particle
    union
    {
        struct
        {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };
        uint8_t color[4];
        void *data;
        struct
        {
            uint16_t u;
            uint16_t v;
        };
    };
    World *world = nullptr;

    Particle() {}
    Particle(World *world) : EntityBase(), life_time(0), physics(0), type(0), size(64), brightness(255), u(0), v(0), world(world) {}

    ~Particle() {}

    void update(float dt);

};

class ParticleSystem
{
public:
    Particle particles[256];
    World *world = nullptr;

    ParticleSystem(World *world)
    {
        this->world = world;
        for (int i = 0; i < 256; i++)
        {
            particles[i] = Particle(world);
        }
    }

    size_t size() { return 256; }

    void update(float dt);
    void add_particle(Particle part);
};

#endif
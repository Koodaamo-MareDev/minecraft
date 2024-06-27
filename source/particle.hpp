#ifndef _PARTICLE_HPP_
#define _PARTICLE_HPP_

#define PPHYSIC_FLAG_GRAVITY (1 << 0)
#define PPHYSIC_FLAG_COLLIDE (1 << 1)
#define PPHYSIC_FLAG_FRICTION (1 << 2)

#define PPHYSIC_FLAG_ALL 0xFF


#include "entity.hpp"

class particle_t : public entity_t
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
    union {
        struct
        {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };
        uint8_t color[4];
        void* data;
        struct {
            uint16_t u;
            uint16_t v;
        };
    };

    particle_t() : entity_t(), life_time(0), physics(0), type(0), size(64), brightness(255), u(0), v(0) {}

    ~particle_t() {}

    void update(float dt) override;
};

class particle_system_t
{
public:
    particle_t particles[256];

    particle_system_t()
    {
        for (int i = 0; i < 256; i++)
        {
            particles[i] = particle_t();
        }
    }

    size_t size() { return 256; }

    void update(float dt);
    void add_particle(particle_t particle);
};

#endif
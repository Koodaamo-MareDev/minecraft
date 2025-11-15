#ifndef VEC2I_HPP
#define VEC2I_HPP

#include <cstdint>
#include "vec3i.hpp"

typedef double vfloat_t;

class Vec2i
{
public:
    int32_t x = 0, y = 0;
    // Constructors, member variables, and other member functions

    bool operator==(Vec2i const &a) const;
    bool operator!=(Vec2i const &a) const;
    Vec2i operator+(Vec2i const &a) const;
    Vec2i &operator+=(Vec2i const &a);
    Vec2i operator-(Vec2i const &a) const;
    Vec2i &operator-=(Vec2i const &a);
    Vec2i operator*(int32_t const &a) const;
    Vec2i operator/(int32_t const &a) const;
    Vec2i operator-() const;
    vfloat_t magnitude() const;
    int32_t sqr_magnitude() const;
    Vec2i(int32_t x, int32_t y) : x(x), y(y) {}
    Vec2i(int32_t xy) : x(xy), y(xy) {}
    Vec2i() : x(0), y(0) {}
    uint32_t hash() const
    {
        return (x * 73856093) ^ (y * 19349663);
    }
    bool operator<(Vec2i const &a) const
    {
        return hash() < a.hash();
    }
};

inline static uint64_t uint32_pair(uint32_t a, uint32_t b)
{
    uint64_t result = 0;
    uint32_t *ptr = reinterpret_cast<uint32_t *>(&result);
    ptr[0] = a;
    ptr[1] = b;
    return result;
}

#endif
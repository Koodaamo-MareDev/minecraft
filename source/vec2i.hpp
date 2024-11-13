#ifndef VEC2I_HPP
#define VEC2I_HPP

#include <cstdint>
#include "vec3i.hpp"

typedef double vfloat_t;

class vec2i
{
public:
    int32_t x = 0, y = 0;
    // Constructors, member variables, and other member functions

    bool operator==(vec2i const &a) const;
    bool operator!=(vec2i const &a) const;
    vec2i operator+(vec2i const &a) const;
    vec2i &operator+=(vec2i const &a);
    vec2i operator-(vec2i const &a) const;
    vec2i &operator-=(vec2i const &a);
    vec2i operator*(int32_t const &a) const;
    vec2i operator/(int32_t const &a) const;
    vec2i operator-() const;
    vfloat_t magnitude() const;
    int32_t sqr_magnitude() const;
    vec2i(int32_t x, int32_t y) : x(x), y(y) {}
    vec2i(int32_t xy) : x(xy), y(xy) {}
    vec2i() : x(0), y(0) {}
    uint32_t hash() const
    {
        return (x * 73856093) ^ (y * 19349663);
    }
    bool operator<(vec2i const &a) const
    {
        return hash() < a.hash();
    }
};

#endif
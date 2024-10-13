#ifndef _VEC3F_HPP_
#define _VEC3F_HPP_
#include "vec3i.hpp"

#include <cmath>
#include <array>

#if HW_RVL
#include "ogc/gx.h"
#endif

class vec3i; // Forward declaration

typedef double vfloat_t;

class vec3f
{
public:
    // Constructors, member variables, and other member functions
    vfloat_t x = 0, y = 0, z = 0;
    bool operator==(vec3f const &a) const;
    vec3f operator+(vec3f const &a) const;
    vec3f operator-(vec3f const &a) const;
    vec3f operator+(vec3i const &a) const;
    vec3f operator-(vec3i const &a) const;
    vec3f operator*(vfloat_t const &a) const;
    vec3f operator/(vfloat_t const &a) const;
    vec3f operator-() const;
    vec3f operator%(vfloat_t const &b);
    vec3f normalize() const;
    vfloat_t magnitude() const;
    vfloat_t sqr_magnitude() const;
    static vec3f lerp(vec3f a, vec3f b, vfloat_t t)
    {
        return a + (b - a) * t;
    }
    vec3f(vfloat_t x, vfloat_t y, vfloat_t z) : x(x), y(y), z(z) {}
    vec3f(vfloat_t xyz) : x(xyz), y(xyz), z(xyz) {}
    vec3f() : x(0), y(0), z(0) {}

    vec3f abs() const
    {
        return vec3f{std::abs(x), std::abs(y), std::abs(z)};
    }

    // Returns the vector split into three axis-aligned vectors
    std::array<vec3f, 3> split() const
    {
        std::array<vec3f, 3> result;
        result[0] = vec3f(x, 0, 0);
        result[1] = vec3f(0, y, 0);
        result[2] = vec3f(0, 0, z);
        return result;
    }

#if HW_RVL
    vec3f(guVector a) : x(a.x), y(a.y), z(a.z) {}
    operator guVector() const;
#endif

private:
    // Member variables
};
inline vec3f operator*(vfloat_t const &a, vec3f const &b)
{
    return b * a;
}
#endif
#ifndef _VEC3F_HPP_
#define _VEC3F_HPP_
#include "vec3i.hpp"

#include <cmath>
#include <array>

#if HW_RVL
#include "ogc/gx.h"
#endif

class Vec3i; // Forward declaration

typedef double vfloat_t;

class Vec3f
{
public:
    // Constructors, member variables, and other member functions
    vfloat_t x = 0, y = 0, z = 0;
    bool operator==(Vec3f const &a) const;
    Vec3f operator+(Vec3f const &a) const;
    Vec3f operator-(Vec3f const &a) const;
    Vec3f operator+(Vec3i const &a) const;
    Vec3f operator-(Vec3i const &a) const;
    Vec3f operator*(vfloat_t const &a) const;
    Vec3f operator/(vfloat_t const &a) const;
    Vec3f operator-() const;
    Vec3f operator%(vfloat_t const &b);
    Vec3f fast_normalize() const; // Fast normalization using fast inverse square root
    Vec3f normalize() const;
    vfloat_t fast_magnitude() const; // Fast magnitude using fast inverse square root
    vfloat_t magnitude() const;
    vfloat_t sqr_magnitude() const;
    static Vec3f lerp(Vec3f a, Vec3f b, vfloat_t t)
    {
        return a + (b - a) * t;
    }
    Vec3f(vfloat_t x, vfloat_t y, vfloat_t z) : x(x), y(y), z(z) {}
    Vec3f(vfloat_t xyz) : x(xyz), y(xyz), z(xyz) {}
    Vec3f() : x(0), y(0), z(0) {}

    Vec3f abs() const
    {
        return Vec3f{std::abs(x), std::abs(y), std::abs(z)};
    }

    Vec3i round() const
    {
        return Vec3i(static_cast<int32_t>(std::round(x)), static_cast<int32_t>(std::round(y)), static_cast<int32_t>(std::round(z)));
    }

    // Returns the vector split into three axis-aligned vectors
    std::array<Vec3f, 3> split() const
    {
        std::array<Vec3f, 3> result;
        result[0] = Vec3f(x, 0, 0);
        result[1] = Vec3f(0, y, 0);
        result[2] = Vec3f(0, 0, z);
        return result;
    }

    operator Vec3i() const;

#if HW_RVL
    Vec3f(guVector a) : x(a.x), y(a.y), z(a.z) {}
    operator guVector() const;
#endif

private:
    // Member variables
};
inline Vec3f operator*(vfloat_t const &a, Vec3f const &b)
{
    return b * a;
}
#endif
#ifndef _VEC3I_HPP_
#define _VEC3I_HPP_
#include <cstdint>
class vec3f; // Forward declaration
typedef double vfloat_t;
class vec3i
{
public:
    int32_t x = 0, y = 0, z = 0;
    // Constructors, member variables, and other member functions

    bool operator==(vec3i const &a) const;
    bool operator!=(vec3i const &a) const;
    vec3i operator+(vec3i const &a) const;
    vec3i& operator+=(vec3i const &a);
    vec3i operator-(vec3i const &a) const;
    vec3i& operator-=(vec3i const &a);
    vec3f operator+(vec3f const &a) const;
    vec3f operator-(vec3f const &a) const;
    vec3i operator*(int32_t const &a) const;
    vec3i operator/(int32_t const &a) const;
    vec3i operator-() const;
    vfloat_t magnitude() const;
    uint32_t sqr_magnitude() const;
    vec3i(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    vec3i(int32_t xyz) : x(xyz), y(xyz), z(xyz) {}
    vec3i() : x(0), y(0), z(0) {}
    uint32_t hash() const
    {
        return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
    }
    bool operator<(vec3i const &a) const
    {
        return hash() < a.hash();
    }
};

inline vec3i operator*(int32_t const &a, vec3i const &b)
{
    return b * a;
}

#endif
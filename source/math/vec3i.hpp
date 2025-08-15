#ifndef _VEC3I_HPP_
#define _VEC3I_HPP_
#include <cstdint>
class Vec3f; // Forward declaration
typedef double vfloat_t;
class Vec3i
{
public:
    int32_t x = 0, y = 0, z = 0;
    // Constructors, member variables, and other member functions

    bool operator==(Vec3i const &a) const;
    bool operator!=(Vec3i const &a) const;
    Vec3i operator+(Vec3i const &a) const;
    Vec3i& operator+=(Vec3i const &a);
    Vec3i operator-(Vec3i const &a) const;
    Vec3i& operator-=(Vec3i const &a);
    Vec3f operator+(Vec3f const &a) const;
    Vec3f operator-(Vec3f const &a) const;
    Vec3i operator*(int32_t const &a) const;
    Vec3i operator/(int32_t const &a) const;
    Vec3i operator-() const;
    vfloat_t magnitude() const;
    uint32_t sqr_magnitude() const;
    Vec3i(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    Vec3i(int32_t xyz) : x(xyz), y(xyz), z(xyz) {}
    Vec3i() : x(0), y(0), z(0) {}
    uint32_t hash() const
    {
        return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
    }
    bool operator<(Vec3i const &a) const
    {
        return hash() < a.hash();
    }
    operator Vec3f() const;
};

inline Vec3i operator*(int32_t const &a, Vec3i const &b)
{
    return b * a;
}

#endif
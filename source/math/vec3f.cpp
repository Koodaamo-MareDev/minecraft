#include "vec3f.hpp"
#include "vec3i.hpp" // Include the necessary header for vec3i if needed
#include <math/math_utils.h>

#if HW_RVL
#include "ogc/gx.h"
#endif

bool Vec3f::operator==(Vec3f const &a) const
{
    return x == a.x && y == a.y && z == a.z;
}

Vec3f Vec3f::operator+(Vec3f const &a) const
{
    return Vec3f{x + a.x, y + a.y, z + a.z};
}

Vec3f Vec3f::operator-(Vec3f const &a) const
{
    return Vec3f{x - a.x, y - a.y, z - a.z};
}

Vec3f Vec3f::operator+(Vec3i const &a) const
{
    return Vec3f{x + a.x, y + a.y, z + a.z};
}

Vec3f Vec3f::operator-(Vec3i const &a) const
{
    return Vec3f{x - a.x, y - a.y, z - a.z};
}

Vec3f Vec3f::operator*(vfloat_t const &a) const
{
    return Vec3f{x * a, y * a, z * a};
}

Vec3f Vec3f::operator/(vfloat_t const &a) const
{
    return Vec3f{x / a, y / a, z / a};
}
Vec3f Vec3f::operator-() const
{
    return Vec3f(-x, -y, -z);
}
Vec3f Vec3f::operator%(vfloat_t const &a)
{
    return Vec3f(std::fmod(x, a), std::fmod(y, a), std::fmod(z, a));
}
Vec3f Vec3f::fast_normalize() const
{
    vfloat_t length = Q_rsqrt_d(x * x + y * y + z * z);
    if (length == 0)
    {
        return Vec3f(0, 0, 0);
    }
    return Vec3f(x * length, y * length, z * length);
}
Vec3f Vec3f::normalize() const
{
    vfloat_t length = std::sqrt(x * x + y * y + z * z);
    if (length == 0)
    {
        return Vec3f(0, 0, 0);
    }
    return Vec3f(x / length, y / length, z / length);
}
vfloat_t Vec3f::fast_magnitude() const
{
    return Q_sqrt_d(x * x + y * y + z * z);
}
vfloat_t Vec3f::magnitude() const
{
    return std::sqrt(x * x + y * y + z * z);
}

vfloat_t Vec3f::sqr_magnitude() const
{
    return x * x + y * y + z * z;
}

Vec3f::operator Vec3i() const
{
    return Vec3i(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z));
}

#if HW_RVL
Vec3f::operator guVector() const
{
    return {float(x), float(y), float(z)};
}
#endif
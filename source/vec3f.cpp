#include "vec3f.hpp"
#include "vec3i.hpp" // Include the necessary header for vec3i if needed

#if HW_RVL
#include "ogc/gx.h"
#endif

bool vec3f::operator==(vec3f const &a) const
{
    return x == a.x && y == a.y && z == a.z;
}

vec3f vec3f::operator+(vec3f const &a) const
{
    return vec3f{x + a.x, y + a.y, z + a.z};
}

vec3f vec3f::operator-(vec3f const &a) const
{
    return vec3f{x - a.x, y - a.y, z - a.z};
}

vec3f vec3f::operator+(vec3i const &a) const
{
    return vec3f{x + a.x, y + a.y, z + a.z};
}

vec3f vec3f::operator-(vec3i const &a) const
{
    return vec3f{x - a.x, y - a.y, z - a.z};
}

vec3f vec3f::operator*(vfloat_t const &a) const
{
    return vec3f{x * a, y * a, z * a};
}

vec3f vec3f::operator/(vfloat_t const &a) const
{
    return vec3f{x / a, y / a, z / a};
}
vec3f vec3f::operator-() const
{
    return vec3f(-x, -y, -z);
}
vec3f vec3f::operator%(vfloat_t const &a)
{
    return vec3f(std::fmod(x, a), std::fmod(y, a), std::fmod(z, a));
}
vec3f vec3f::normalize() const
{
    vfloat_t length = std::sqrt(x * x + y * y + z * z);
    if (length == 0)
    {
        return vec3f(0, 0, 0);
    }
    return vec3f(x / length, y / length, z / length);
}

vfloat_t vec3f::magnitude() const
{
    return std::sqrt(x * x + y * y + z * z);
}

vfloat_t vec3f::sqr_magnitude() const
{
    return x * x + y * y + z * z;
}

#if HW_RVL
vec3f::operator guVector() const
{
    return {float(x), float(y), float(z)};
}
#endif
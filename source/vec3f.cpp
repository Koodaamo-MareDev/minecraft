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

vec3f vec3f::operator*(float const &a) const
{
    return vec3f{x * a, y * a, z * a};
}

vec3f vec3f::operator/(float const &a) const
{
    return vec3f{x / a, y / a, z / a};
}
vec3f vec3f::operator-() const
{
    return vec3f(-x, -y, -z);
}
#if HW_RVL
vec3f::operator guVector()
{
    return {x, y, z};
}
#endif
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
#if HW_RVL
guVector vec3f::operator()()
{
    return {x, y, z};
}
#endif
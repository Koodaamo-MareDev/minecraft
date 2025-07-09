#include "vec3i.hpp"
#include "vec3f.hpp" // Include the necessary header for vec3f if needed
#include <math/math_utils.h>
bool vec3i::operator==(vec3i const &a) const
{
    return x == a.x && y == a.y && z == a.z;
}

bool vec3i::operator!=(vec3i const &a) const
{
    return x != a.x || y != a.y || z != a.z;
}

vec3i vec3i::operator+(vec3i const &a) const
{
    return vec3i{x + a.x, y + a.y, z + a.z};
}

vec3i &vec3i::operator+=(vec3i const &a)
{
    this->x += a.x;
    this->y += a.y;
    this->z += a.z;
    return *this;
}

vec3i vec3i::operator-(vec3i const &a) const
{
    return vec3i{x - a.x, y - a.y, z - a.z};
}

vec3i &vec3i::operator-=(vec3i const &a)
{
    this->x -= a.x;
    this->y -= a.y;
    this->z -= a.z;
    return *this;
}

vec3f vec3i::operator+(vec3f const &a) const
{
    return vec3f{x + a.x, y + a.y, z + a.z};
}

vec3f vec3i::operator-(vec3f const &a) const
{
    return vec3f{x - a.x, y - a.y, z - a.z};
}

vec3i vec3i::operator*(int32_t const &a) const
{
    return vec3i{x * a, y * a, z * a};
}

vec3i vec3i::operator/(int32_t const &a) const
{
    return vec3i{x / a, y / a, z / a};
}

vec3i vec3i::operator-() const
{
    return vec3i{-x, -y, -z};
}

vfloat_t vec3i::magnitude() const
{
    return Q_sqrt_d(x * x + y * y + z * z);
}

uint32_t vec3i::sqr_magnitude() const
{
    return uint32_t(x * x) + uint32_t(y * y) + uint32_t(z * z);
}

vec3i::operator vec3f() const
{
    return vec3f(static_cast<vfloat_t>(x), static_cast<vfloat_t>(y), static_cast<vfloat_t>(z));
}

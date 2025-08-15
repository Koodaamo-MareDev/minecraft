#include "vec3i.hpp"
#include "vec3f.hpp" // Include the necessary header for vec3f if needed
#include <math/math_utils.h>
bool Vec3i::operator==(Vec3i const &a) const
{
    return x == a.x && y == a.y && z == a.z;
}

bool Vec3i::operator!=(Vec3i const &a) const
{
    return x != a.x || y != a.y || z != a.z;
}

Vec3i Vec3i::operator+(Vec3i const &a) const
{
    return Vec3i{x + a.x, y + a.y, z + a.z};
}

Vec3i &Vec3i::operator+=(Vec3i const &a)
{
    this->x += a.x;
    this->y += a.y;
    this->z += a.z;
    return *this;
}

Vec3i Vec3i::operator-(Vec3i const &a) const
{
    return Vec3i{x - a.x, y - a.y, z - a.z};
}

Vec3i &Vec3i::operator-=(Vec3i const &a)
{
    this->x -= a.x;
    this->y -= a.y;
    this->z -= a.z;
    return *this;
}

Vec3f Vec3i::operator+(Vec3f const &a) const
{
    return Vec3f{x + a.x, y + a.y, z + a.z};
}

Vec3f Vec3i::operator-(Vec3f const &a) const
{
    return Vec3f{x - a.x, y - a.y, z - a.z};
}

Vec3i Vec3i::operator*(int32_t const &a) const
{
    return Vec3i{x * a, y * a, z * a};
}

Vec3i Vec3i::operator/(int32_t const &a) const
{
    return Vec3i{x / a, y / a, z / a};
}

Vec3i Vec3i::operator-() const
{
    return Vec3i{-x, -y, -z};
}

vfloat_t Vec3i::magnitude() const
{
    return Q_sqrt_d(x * x + y * y + z * z);
}

uint32_t Vec3i::sqr_magnitude() const
{
    return uint32_t(x * x) + uint32_t(y * y) + uint32_t(z * z);
}

Vec3i::operator Vec3f() const
{
    return Vec3f(static_cast<vfloat_t>(x), static_cast<vfloat_t>(y), static_cast<vfloat_t>(z));
}

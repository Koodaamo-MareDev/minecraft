#include "vec2i.hpp"

#include <math/math_utils.h>

bool Vec2i::operator==(Vec2i const &a) const
{
    return x == a.x && y == a.y;
}

bool Vec2i::operator!=(Vec2i const &a) const
{
    return x != a.x || y != a.y;
}

Vec2i Vec2i::operator+(Vec2i const &a) const
{
    return Vec2i(x + a.x, y + a.y);
}

Vec2i &Vec2i::operator+=(Vec2i const &a)
{
    x += a.x;
    y += a.y;
    return *this;
}

Vec2i Vec2i::operator-(Vec2i const &a) const
{
    return Vec2i(x - a.x, y - a.y);
}

Vec2i &Vec2i::operator-=(Vec2i const &a)
{
    x -= a.x;
    y -= a.y;
    return *this;
}

Vec2i Vec2i::operator*(int32_t const &a) const
{
    return Vec2i(x * a, y * a);
}

Vec2i Vec2i::operator/(int32_t const &a) const
{
    return Vec2i(x / a, y / a);
}

Vec2i Vec2i::operator-() const
{
    return Vec2i(-x, -y);
}

vfloat_t Vec2i::magnitude() const
{
    return Q_sqrt_d(x * x + y * y);
}

int32_t Vec2i::sqr_magnitude() const
{
    return x * x + y * y;
}


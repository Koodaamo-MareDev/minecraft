#include "vec2i.hpp"

#include "maths.hpp"

bool vec2i::operator==(vec2i const &a) const
{
    return x == a.x && y == a.y;
}

bool vec2i::operator!=(vec2i const &a) const
{
    return x != a.x || y != a.y;
}

vec2i vec2i::operator+(vec2i const &a) const
{
    return vec2i(x + a.x, y + a.y);
}

vec2i &vec2i::operator+=(vec2i const &a)
{
    x += a.x;
    y += a.y;
    return *this;
}

vec2i vec2i::operator-(vec2i const &a) const
{
    return vec2i(x - a.x, y - a.y);
}

vec2i &vec2i::operator-=(vec2i const &a)
{
    x -= a.x;
    y -= a.y;
    return *this;
}

vec2i vec2i::operator*(int32_t const &a) const
{
    return vec2i(x * a, y * a);
}

vec2i vec2i::operator/(int32_t const &a) const
{
    return vec2i(x / a, y / a);
}

vec2i vec2i::operator-() const
{
    return vec2i(-x, -y);
}

vfloat_t vec2i::magnitude() const
{
    return Q_sqrt_d(x * x + y * y);
}

int32_t vec2i::sqr_magnitude() const
{
    return x * x + y * y;
}


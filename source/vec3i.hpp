#ifndef _VEC3I_HPP_
#define _VEC3I_HPP_
#include <cstdint>
class vec3f; // Forward declaration
typedef double vfloat_t;
class vec3i
{
public:
    int x = 0, y = 0, z = 0;
    // Constructors, member variables, and other member functions

    bool operator==(vec3i const &a) const;
    bool operator!=(vec3i const &a) const;
    vec3i operator+(vec3i const &a) const;
    vec3i& operator+=(vec3i const &a);
    vec3i operator-(vec3i const &a) const;
    vec3i& operator-=(vec3i const &a);
    vec3f operator+(vec3f const &a) const;
    vec3f operator-(vec3f const &a) const;
    vec3i operator*(int const &a) const;
    vec3i operator/(int const &a) const;
    vec3i operator-() const;
    vfloat_t magnitude() const;
    vfloat_t sqr_magnitude() const;
    vec3i(int x, int y, int z) : x(x), y(y), z(z) {}
    vec3i(int xyz) : x(xyz), y(xyz), z(xyz) {}
    vec3i() : x(0), y(0), z(0) {}
    uint32_t hash() const
    {
        return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
    }
    bool operator<(vec3i const &a) const
    {
        return hash() < a.hash();
    }

private:
    // Member variables
};

inline vec3i operator*(int const &a, vec3i const &b)
{
    return b * a;
}

inline vec3i block_to_chunk_pos(vec3i pos)
{
    pos.x &= ~0xF;
    pos.z &= ~0xF;
    pos.x /= 16;
    pos.z /= 16;
    pos.y = 0;
    return pos;
}

#endif
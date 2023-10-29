#ifndef _VEC3F_HPP_
#define _VEC3F_HPP_
#include "vec3i.hpp"

class vec3i; // Forward declaration

class vec3f
{
public:
    // Constructors, member variables, and other member functions
    float x = 0, y = 0, z = 0;
    bool operator==(vec3f const &a) const;
    vec3f operator+(vec3f const &a) const;
    vec3f operator-(vec3f const &a) const;
    vec3f operator+(vec3i const &a) const;
    vec3f operator-(vec3i const &a) const;
    vec3f(float x, float y, float z) : x(x), y(y), z(z) {}
    vec3f(int xyz) : x(xyz), y(xyz), z(xyz) {}
    vec3f() : x(0), y(0), z(0) {}

private:
    // Member variables
};
#endif
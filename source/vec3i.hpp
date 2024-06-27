#ifndef _VEC3I_HPP_
#define _VEC3I_HPP_
#include <cstdint>
class vec3f;  // Forward declaration

class vec3i {
public:
    int x = 0, y = 0, z = 0;
    // Constructors, member variables, and other member functions
    
    bool operator==(vec3i const& a) const;
    bool operator!=(vec3i const& a) const;
    vec3i operator+(vec3i const& a) const;
    vec3i operator-(vec3i const& a) const;
    vec3f operator+(vec3f const& a) const;
    vec3f operator-(vec3f const& a) const;
    vec3i operator*(int const& a) const;
    vec3i operator/(int const& a) const;
    vec3i operator-() const;
    vec3i(int x, int y, int z) : x(x), y(y), z(z) {}
    vec3i(int xyz) : x(xyz), y(xyz), z(xyz) {}
    vec3i() : x(0), y(0), z(0) {}

private:
    // Member variables
};

inline vec3i operator*(int const &a, vec3i const &b)
{
    return b * a;
}
#endif
#ifndef _VEC3D_HPP_
#define _VEC3D_HPP_
struct vec3d
{
    double x = 0, y = 0, z = 0;
    bool operator==(vec3d const& a) const {
        return x == a.x && y == a.y && z == a.z;
    }
    vec3d operator+(vec3d const& a) const {
        return {x + a.x, y + a.y, z + a.z};
    }
    vec3d operator-(vec3d const& a) const {
        return {x - a.x, y - a.y, z - a.z};
    }
};
#endif
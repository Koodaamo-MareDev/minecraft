#ifndef _AABB_HPP_
#define _AABB_HPP_

#include "vec3f.hpp"

class aabb_t
{
public:
    vec3f min;
    vec3f max;

    aabb_t() : min(0, 0, 0), max(0, 0, 0) {}
    aabb_t(vec3f min, vec3f max) : min(min), max(max) {}

    bool intersects(aabb_t other)
    {
        return (this->min.x <= other.max.x && this->max.x >= other.min.x) &&
               (this->min.y <= other.max.y && this->max.y >= other.min.y) &&
               (this->min.z <= other.max.z && this->max.z >= other.min.z);
    }

    bool contains(vec3f point)
    {
        return (point.x >= this->min.x && point.x <= this->max.x) &&
               (point.y >= this->min.y && point.y <= this->max.y) &&
               (point.z >= this->min.z && point.z <= this->max.z);
    }

    vec3f push_out(aabb_t other);
};

#endif
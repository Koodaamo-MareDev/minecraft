#include "aabb.hpp"

vec3f aabb_t::push_out(aabb_t other)
{
    // Check if the two AABBs intersect
    if (!this->intersects(other))
    {
        return vec3f(0, 0, 0);
    }

    // Calculate the vector to push out the AABB
    vec3f push = vec3f(0, 0, 0);

    // Calculate the distance to push out the AABB
    float dx1 = other.max.x - this->min.x;
    float dx2 = other.min.x - this->max.x;
    float dy1 = other.max.y - this->min.y;
    float dy2 = other.min.y - this->max.y;
    float dz1 = other.max.z - this->min.z;
    float dz2 = other.min.z - this->max.z;

    // Calculate the minimum distance to push out the AABB
    float dx = (std::abs(dx1) < std::abs(dx2)) ? dx1 : dx2;
    float dy = (std::abs(dy1) < std::abs(dy2)) ? dy1 : dy2;
    float dz = (std::abs(dz1) < std::abs(dz2)) ? dz1 : dz2;

    // Push out the AABB
    if (std::abs(dx) < std::abs(dy) && std::abs(dx) < std::abs(dz))
    {
        push.x = dx;
    }
    else if (std::abs(dy) < std::abs(dz))
    {
        push.y = dy;
    }
    else
    {
        push.z = dz;
    }

    return push;
}
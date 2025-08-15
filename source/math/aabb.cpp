#include "aabb.hpp"

Vec3f AABB::push_out(AABB other)
{
    // Check if the two AABBs intersect
    if (!this->intersects(other))
    {
        return Vec3f(0, 0, 0);
    }

    // Calculate the vector to push out the AABB
    Vec3f push = Vec3f(0, 0, 0);

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

Vec3f AABB::push_out_horizontal(AABB other)
{
    // Check if the two AABBs intersect
    if (!this->intersects(other))
    {
        return Vec3f(0, 0, 0);
    }

    // Initialize the push vector
    Vec3f push = Vec3f(0, 0, 0);

    // Calculate the distance to push out the AABB
    float dx1 = other.max.x - this->min.x;
    float dx2 = other.min.x - this->max.x;
    float dz1 = other.max.z - this->min.z;
    float dz2 = other.min.z - this->max.z;

    // Calculate the minimum distance to push out the AABB
    float dx = (std::abs(dx1) < std::abs(dx2)) ? dx1 : dx2;
    float dz = (std::abs(dz1) < std::abs(dz2)) ? dz1 : dz2;

    // Push out the AABB
    if (std::abs(dx) < std::abs(dz))
    {
        push.x = dx;
    }
    else
    {
        push.z = dz;
    }

    return push;
}
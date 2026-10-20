#pragma once

#include <math/vec3f.hpp>

class EntityBase
{
public:
    Vec3f position;
    Vec3f velocity;

    EntityBase() : position(0, 0, 0), velocity(0, 0, 0) {}
    EntityBase(Vec3f position, Vec3f velocity) : position(position), velocity(velocity) {}
    EntityBase(Vec3f position) : position(position), velocity(0, 0, 0) {}
    virtual ~EntityBase() {}
};
#ifndef _AABB_HPP_
#define _AABB_HPP_

#include "vec3f.hpp"

class AABB
{
public:
    Vec3f min;
    Vec3f max;

    AABB() : min(0, 0, 0), max(0, 0, 0) {}
    AABB(Vec3f min, Vec3f max) : min(min), max(max) {}

    bool intersects(AABB other)
    {
        return (this->min.x < other.max.x && this->max.x > other.min.x) &&
               (this->min.y < other.max.y && this->max.y > other.min.y) &&
               (this->min.z < other.max.z && this->max.z > other.min.z);
    }

    bool contains(Vec3f point)
    {
        return (point.x >= this->min.x && point.x <= this->max.x) &&
               (point.y >= this->min.y && point.y <= this->max.y) &&
               (point.z >= this->min.z && point.z <= this->max.z);
    }

    AABB extend_by(Vec3f offset)
    {
        AABB result;
        if (offset.x < 0)
        {
            result.min.x = this->min.x + offset.x;
            result.max.x = this->max.x;
        }
        else
        {
            result.min.x = this->min.x;
            result.max.x = this->max.x + offset.x;
        }
        if (offset.y < 0)
        {
            result.min.y = this->min.y + offset.y;
            result.max.y = this->max.y;
        }
        else
        {
            result.min.y = this->min.y;
            result.max.y = this->max.y + offset.y;
        }
        if (offset.z < 0)
        {
            result.min.z = this->min.z + offset.z;
            result.max.z = this->max.z;
        }
        else
        {
            result.min.z = this->min.z;
            result.max.z = this->max.z + offset.z;
        }
        return result;
    }

    vfloat_t calculate_x_offset(const AABB &other, vfloat_t offset)
    {
        if (other.max.y <= this->min.y || other.min.y >= this->max.y)
            return offset;
        if (other.max.z <= this->min.z || other.min.z >= this->max.z)
            return offset;
        if (offset > 0 && other.max.x <= this->min.x)
        {
            vfloat_t d = this->min.x - other.max.x;
            if (d < offset)
                offset = d;
        }
        if (offset < 0 && other.min.x >= this->max.x)
        {
            vfloat_t d = this->max.x - other.min.x;
            if (d > offset)
                offset = d;
        }
        return offset;
    }

    vfloat_t calculate_y_offset(const AABB &other, vfloat_t offset)
    {
        if (other.max.x <= this->min.x || other.min.x >= this->max.x)
            return offset;
        if (other.max.z <= this->min.z || other.min.z >= this->max.z)
            return offset;
        if (offset > 0 && other.max.y <= this->min.y)
        {
            vfloat_t d = this->min.y - other.max.y;
            if (d < offset)
                offset = d;
        }
        if (offset < 0 && other.min.y >= this->max.y)
        {
            vfloat_t d = this->max.y - other.min.y;
            if (d > offset)
                offset = d;
        }
        return offset;
    }

    vfloat_t calculate_z_offset(const AABB &other, vfloat_t offset)
    {
        if (other.max.x <= this->min.x || other.min.x >= this->max.x)
            return offset;
        if (other.max.y <= this->min.y || other.min.y >= this->max.y)
            return offset;
        if (offset > 0 && other.max.z <= this->min.z)
        {
            vfloat_t d = this->min.z - other.max.z;
            if (d < offset)
                offset = d;
        }
        if (offset < 0 && other.min.z >= this->max.z)
        {
            vfloat_t d = this->max.z - other.min.z;
            if (d > offset)
                offset = d;
        }
        return offset;
    }

    void translate(Vec3f offset)
    {
        this->min = this->min + offset;
        this->max = this->max + offset;
    }

    AABB inflate(vfloat_t amount)
    {
        return AABB(this->min - Vec3f(amount, amount, amount), this->max + Vec3f(amount, amount, amount));
    }

    Vec3f push_out(AABB other);

    Vec3f push_out_horizontal(AABB other);
};

#endif
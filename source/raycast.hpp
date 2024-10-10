#ifndef _RAYCAST_H_
#define _RAYCAST_H_

#include <cmath>
#include <vector>
#include "vec3f.hpp"
#include "vec3i.hpp"
#include "chunk_new.hpp"
#include "blocks.hpp"
#include "ported/JavaRandom.hpp"
#include "maths.hpp"

template <typename T>
inline int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

inline double mod(double val, double modulus)
{
    return std::fmod((std::fmod(val, modulus) + modulus), modulus);
}

inline double intbound(double s, double ds)
{
    // Find the smallest positive t such that s+t*ds is an integer.
    if (ds < 0)
    {
        return intbound(-s, -ds);
    }
    else
    {
        s = mod(s, 1);
        // problem is now s+t*ds = 1
        return (1 - s) / ds;
    }
}
inline int checkabove(vec3i pos, chunk_t *chunk = nullptr)
{
    block_t *block = nullptr;
    chunk = chunk ? chunk : get_chunk_from_pos(pos, false, false);
    if (!chunk)
        return pos.y;
    // Starting from the y coordinate, cast a ray up that stops at the first (partially) opaque block.
    pos.y++;
    while (pos.y < 255 && (block = chunk->get_block(pos)) && properties(block->get_blockid()).m_collision == CollisionType::none)
        pos.y++;
    // This should return 255 at most which means the ray hit the world height limit
    return pos.y;
}
inline int checkbelow(vec3i pos, chunk_t *chunk = nullptr)
{
    block_t *block = nullptr;
    chunk = chunk ? chunk : get_chunk_from_pos(pos, false, false);
    if (!chunk)
        return pos.y;
    // Starting from the y coordinate, cast a ray down that stops at the first (partially) opaque block.
    pos.y--;
    while (pos.y > 0 && (block = chunk->get_block(pos)) && properties(block->get_blockid()).m_collision == CollisionType::none)
        pos.y--;
    // This should return 0 at most which means the ray hit the bedrock
    return pos.y;
}
inline int skycast(vec3i pos, chunk_t *chunk = nullptr)
{
    block_t *block = nullptr;
    chunk = chunk ? chunk : get_chunk_from_pos(pos, false, false);
    if (!chunk)
        return -9999;
    // Starting from world height limit, cast a ray down that stops at the first (partially) opaque block.
    pos.y = 255;
    while (pos.y > 0 && (block = chunk->get_block(pos)) && !get_block_opacity(block->get_blockid()))
        pos.y--;
    // If the cast went out of the world bounds, tell it to the caller by returning -9999
    if (!block)
        return -9999;
    return pos.y;
}

inline bool raycast(
    vec3f origin,
    vec3f direction,
    float dst,
    vec3i *output,
    vec3i *output_face)
{
    // From "A Fast Voxel Traversal Algorithm for Ray Tracing"
    // by John Amanatides and Andrew Woo, 1987
    // <http://www.cse.yorku.ca/~amana/research/grid.pdf>
    // <http://citeseer.ist.psu.edu/viewdoc/summary?doi=10.1.1.42.3443>
    // Extensions to the described algorithm:
    //   • Imposed a distance limit.
    //   • The face passed through to reach the current cube is provided to
    //     the callback.

    // The foundation of this algorithm is a parameterized representation of
    // the provided ray,
    //                    origin + t * direction,
    // except that t is not actually stored; rather, at any given point in the
    // traversal, we keep track of the *greater* t values which we would have
    // if we took a step sufficient to cross a cube boundary along that axis
    // (i.e. change the integer part of the coordinate) in the variables
    // tMaxX, tMaxY, and tMaxZ.

    // Cube containing origin point.
    double x = std::floor(origin.x);
    double y = std::floor(origin.y);
    double z = std::floor(origin.z);

    // Break out direction vector.
    double dx = direction.x;
    double dy = direction.y;
    double dz = direction.z;

    // Direction to increment x,y,z when stepping.
    double stepX = sgn(dx);
    double stepY = sgn(dy);
    double stepZ = sgn(dz);

    // See description above. The initial values depend on the fractional
    // part of the origin.
    double tMaxX = intbound(origin.x, dx);
    double tMaxY = intbound(origin.y, dy);
    double tMaxZ = intbound(origin.z, dz);

    // The change in t when taking a step (always positive).
    double tDeltaX = stepX / dx;
    double tDeltaY = stepY / dy;
    double tDeltaZ = stepZ / dz;

    // Buffer for reporting faces to the callback.
    vec3i face = vec3i(0, 0, 0);

    // Avoids an infinite loop.
    if (dx == 0 && dy == 0 && dz == 0)
        return false;

    // Rescale from units of 1 cube-edge to units of 'direction' so we can
    // compare with 't'.
    float radius = dst * Q_rsqrt(dx * dx + dy * dy + dz * dz);
    // float radius = dst;
    //  Deal with world bounds or their absence.
    vec3i maxvec = vec3i(int(origin.x) + (dst + 1), int(origin.y) + (dst + 1), int(origin.z) + (dst + 1));
    vec3i minvec = vec3i(int(origin.x) - (dst + 1), int(origin.y) - (dst + 1), int(origin.z) - (dst + 1));

    while (true)
    {
        if (!(x < minvec.x || y < minvec.y || z < minvec.z || x >= maxvec.x || y >= maxvec.y || z >= maxvec.z))
        {
            vec3i block_pos = vec3i(int(x), int(y), int(z));
            block_t *block = get_block_at(block_pos);
            if (block)
            {
                BlockID blockid = block->get_blockid();
                if (blockid != BlockID::air && !is_fluid(blockid))
                {
                    if (output)
                        *output = vec3i(int(x), int(y), int(z));
                    if (get_block_at(block_pos + face) && output_face)
                        *output_face = face;
                    return true;
                }
            }
        }

        // tMaxX stores the t-value at which we cross a cube boundary along the
        // X axis, and similarly for Y and Z. Therefore, choosing the least tMax
        // chooses the closest cube boundary. Only the first case of the four
        // has been commented in detail.
        if (tMaxX < tMaxY)
        {
            if (tMaxX < tMaxZ)
            {
                if (tMaxX > radius)
                    break;
                // Update which cube we are now in.
                x += stepX;
                // Adjust tMaxX to the next X-oriented boundary crossing.
                tMaxX += tDeltaX;
                // Record the normal vector of the cube face we entered.
                face.x = -stepX;
                face.y = 0;
                face.z = 0;
            }
            else
            {
                if (tMaxZ > radius)
                    break;
                z += stepZ;
                tMaxZ += tDeltaZ;
                face.x = 0;
                face.y = 0;
                face.z = -stepZ;
            }
        }
        else
        {
            if (tMaxY < tMaxZ)
            {
                if (tMaxY > radius)
                    break;
                y += stepY;
                tMaxY += tDeltaY;
                face.x = 0;
                face.y = -stepY;
                face.z = 0;
            }
            else
            {
                // Identical to the second case, repeated for simplicity in
                // the conditionals.
                if (tMaxZ > radius)
                    break;
                z += stepZ;
                tMaxZ += tDeltaZ;
                face.x = 0;
                face.y = 0;
                face.z = -stepZ;
            }
        }
    }
    return false;
}

inline bool raycast_inverse(
    vec3f origin,
    vec3f direction,
    float dst,
    vec3i *output,
    vec3i *output_face)
{
    // Cube containing origin point.
    double x = std::floor(origin.x);
    double y = std::floor(origin.y);
    double z = std::floor(origin.z);

    // Break out direction vector.
    double dx = direction.x;
    double dy = direction.y;
    double dz = direction.z;

    // Direction to increment x,y,z when stepping.
    double stepX = sgn(dx);
    double stepY = sgn(dy);
    double stepZ = sgn(dz);

    // See description above. The initial values depend on the fractional
    // part of the origin.
    double tMaxX = intbound(origin.x, dx);
    double tMaxY = intbound(origin.y, dy);
    double tMaxZ = intbound(origin.z, dz);

    // The change in t when taking a step (always positive).
    double tDeltaX = stepX / dx;
    double tDeltaY = stepY / dy;
    double tDeltaZ = stepZ / dz;

    // Buffer for reporting faces to the callback.
    vec3i face = vec3i(0, 0, 0);

    // Avoids an infinite loop.
    if (dx == 0 && dy == 0 && dz == 0)
        return false;

    // Rescale from units of 1 cube-edge to units of 'direction' so we can
    // compare with 't'.
    float radius = dst * Q_rsqrt(dx * dx + dy * dy + dz * dz);
    //  Deal with world bounds or their absence.
    vec3i maxvec = vec3i(int(origin.x) + (dst + 1), int(origin.y) + (dst + 1), int(origin.z) + (dst + 1));
    vec3i minvec = vec3i(int(origin.x) - (dst + 1), int(origin.y) - (dst + 1), int(origin.z) - (dst + 1));

    while (true)
    {
        if (!(x < minvec.x || y < minvec.y || z < minvec.z || x >= maxvec.x || y >= maxvec.y || z >= maxvec.z))
        {
            vec3i block_pos = vec3i(int(x), int(y), int(z));
            block_t *block = get_block_at(block_pos);
            if (block)
            {
                BlockID blockid = block->get_blockid();
                if (blockid == BlockID::air || is_fluid(blockid))
                {
                    if (output)
                        *output = vec3i(int(x), int(y), int(z));
                    if (get_block_at(block_pos + face) && output_face)
                        *output_face = face;
                    return true;
                }
            }
        }

        // tMaxX stores the t-value at which we cross a cube boundary along the
        // X axis, and similarly for Y and Z. Therefore, choosing the least tMax
        // chooses the closest cube boundary. Only the first case of the four
        // has been commented in detail.
        if (tMaxX < tMaxY)
        {
            if (tMaxX < tMaxZ)
            {
                if (tMaxX > radius)
                    break;
                // Update which cube we are now in.
                x += stepX;
                // Adjust tMaxX to the next X-oriented boundary crossing.
                tMaxX += tDeltaX;
                // Record the normal vector of the cube face we entered.
                face.x = -stepX;
                face.y = 0;
                face.z = 0;
            }
            else
            {
                if (tMaxZ > radius)
                    break;
                z += stepZ;
                tMaxZ += tDeltaZ;
                face.x = 0;
                face.y = 0;
                face.z = -stepZ;
            }
        }
        else
        {
            if (tMaxY < tMaxZ)
            {
                if (tMaxY > radius)
                    break;
                y += stepY;
                tMaxY += tDeltaY;
                face.x = 0;
                face.y = -stepY;
                face.z = 0;
            }
            else
            {
                // Identical to the second case, repeated for simplicity in
                // the conditionals.
                if (tMaxZ > radius)
                    break;
                z += stepZ;
                tMaxZ += tDeltaZ;
                face.x = 0;
                face.y = 0;
                face.z = -stepZ;
            }
        }
    }
    return false;
}

inline bool raycast_aabb(
    vec3f origin,
    vec3f direction,
    float dst,
    vec3i *output,
    vec3i *output_face, const aabb_t &aabb)
{
    vfloat_t t1 = (aabb.min.x - origin.x) / direction.x;
    vfloat_t t2 = (aabb.max.x - origin.x) / direction.x;
    vfloat_t t3 = (aabb.min.y - origin.y) / direction.y;
    vfloat_t t4 = (aabb.max.y - origin.y) / direction.y;
    vfloat_t t5 = (aabb.min.z - origin.z) / direction.z;
    vfloat_t t6 = (aabb.max.z - origin.z) / direction.z;

    vfloat_t tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    vfloat_t tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    if (tmax < 0)
    {
        return false;
    }

    if (tmin > tmax)
    {
        return false;
    }

    vfloat_t t = tmin < 0 ? tmax : tmin;

    if (t > dst)
        return false;

    if (output_face)
    {
        if (t == t1)
            *output_face = vec3i(-1, 0, 0);
        else if (t == t2)
            *output_face = vec3i(1, 0, 0);
        else if (t == t3)
            *output_face = vec3i(0, -1, 0);
        else if (t == t4)
            *output_face = vec3i(0, 1, 0);
        else if (t == t5)
            *output_face = vec3i(0, 0, -1);
        else if (t == t6)
            *output_face = vec3i(0, 0, 1);
    }
    return true;
}

inline bool raycast_precise(
    vec3f origin,
    vec3f direction,
    float dst,
    vec3i *output,
    vec3i *output_face,
    std::vector<aabb_t> &aabbs)
{
    // From "A Fast Voxel Traversal Algorithm for Ray Tracing"
    // by John Amanatides and Andrew Woo, 1987
    // <http://www.cse.yorku.ca/~amana/research/grid.pdf>
    // <http://citeseer.ist.psu.edu/viewdoc/summary?doi=10.1.1.42.3443>
    // Extensions to the described algorithm:
    //   • Imposed a distance limit.
    //   • The face passed through to reach the current cube is provided to
    //     the callback.

    // The foundation of this algorithm is a parameterized representation of
    // the provided ray,
    //                    origin + t * direction,
    // except that t is not actually stored; rather, at any given point in the
    // traversal, we keep track of the *greater* t values which we would have
    // if we took a step sufficient to cross a cube boundary along that axis
    // (i.e. change the integer part of the coordinate) in the variables
    // tMaxX, tMaxY, and tMaxZ.

    // Cube containing origin point.
    double x = std::floor(origin.x);
    double y = std::floor(origin.y);
    double z = std::floor(origin.z);

    // Break out direction vector.
    double dx = direction.x;
    double dy = direction.y;
    double dz = direction.z;

    // Direction to increment x,y,z when stepping.
    double stepX = sgn(dx);
    double stepY = sgn(dy);
    double stepZ = sgn(dz);

    // See description above. The initial values depend on the fractional
    // part of the origin.
    double tMaxX = intbound(origin.x, dx);
    double tMaxY = intbound(origin.y, dy);
    double tMaxZ = intbound(origin.z, dz);

    // The change in t when taking a step (always positive).
    double tDeltaX = stepX / dx;
    double tDeltaY = stepY / dy;
    double tDeltaZ = stepZ / dz;

    // Buffer for reporting faces to the callback.
    vec3i face = vec3i(0, 0, 0);

    // Avoids an infinite loop.
    if (dx == 0 && dy == 0 && dz == 0)
        return false;

    // Rescale from units of 1 cube-edge to units of 'direction' so we can
    // compare with 't'.
    float radius = dst * Q_rsqrt(dx * dx + dy * dy + dz * dz);
    // float radius = dst;
    //  Deal with world bounds or their absence.
    vec3i maxvec = vec3i(int(origin.x) + (dst + 1), int(origin.y) + (dst + 1), int(origin.z) + (dst + 1));
    vec3i minvec = vec3i(int(origin.x) - (dst + 1), int(origin.y) - (dst + 1), int(origin.z) - (dst + 1));

    aabbs.clear();
    while (true)
    {
        if (!(x < minvec.x || y < minvec.y || z < minvec.z || x >= maxvec.x || y >= maxvec.y || z >= maxvec.z))
        {
            vec3i block_pos = vec3i(x, y, z);
            block_t *block = get_block_at(block_pos);
            if (block)
            {
                BlockID blockid = block->get_blockid();
                if (blockid != BlockID::air && !is_fluid(blockid))
                {
                    aabb_t aabb;
                    aabb.min = vec3f(block_pos.x, block_pos.y, block_pos.z);
                    aabb.max = aabb.min + vec3f(1, 1, 1);
                    block->get_aabb(block_pos, aabb, aabbs);
                    if (properties(block->id).m_render_type == RenderType::full)
                    {
                        if (output)
                            *output = block_pos;
                        if (get_block_at(block_pos + face) && output_face)
                            *output_face = face;
                        return true;
                    }
                    else
                    {
                        block->get_aabb(block_pos, aabb, aabbs);
                        for (aabb_t m_aabb : aabbs)
                        {
                            if (raycast_aabb(origin, direction, dst, output, output_face, m_aabb))
                            {
                                *output = block_pos;
                                return true;
                            }
                        }
                    }
                    aabbs.clear();
                }
            }
        }

        // tMaxX stores the t-value at which we cross a cube boundary along the
        // X axis, and similarly for Y and Z. Therefore, choosing the least tMax
        // chooses the closest cube boundary. Only the first case of the four
        // has been commented in detail.
        if (tMaxX < tMaxY)
        {
            if (tMaxX < tMaxZ)
            {
                if (tMaxX > radius)
                    break;
                // Update which cube we are now in.
                x += stepX;
                // Adjust tMaxX to the next X-oriented boundary crossing.
                tMaxX += tDeltaX;
                // Record the normal vector of the cube face we entered.
                face.x = -stepX;
                face.y = 0;
                face.z = 0;
            }
            else
            {
                if (tMaxZ > radius)
                    break;
                z += stepZ;
                tMaxZ += tDeltaZ;
                face.x = 0;
                face.y = 0;
                face.z = -stepZ;
            }
        }
        else
        {
            if (tMaxY < tMaxZ)
            {
                if (tMaxY > radius)
                    break;
                y += stepY;
                tMaxY += tDeltaY;
                face.x = 0;
                face.y = -stepY;
                face.z = 0;
            }
            else
            {
                // Identical to the second case, repeated for simplicity in
                // the conditionals.
                if (tMaxZ > radius)
                    break;
                z += stepZ;
                tMaxZ += tDeltaZ;
                face.x = 0;
                face.y = 0;
                face.z = -stepZ;
            }
        }
    }
    return false;
}
inline void explode_raycast(vec3f origin, vec3f direction, float intensity, chunk_t *near)
{
    // Avoids an infinite loop.
    if (direction.sqr_magnitude() < 0.001)
        return;

    // Rescale from units of 1 cube-edge to units of 'direction' so we can
    // compare with 't'.
    float radius = 0.3f * Q_rsqrt(direction.sqr_magnitude());

    vec3f pos = origin;
    vec3f dir = direction * radius;
    while (true)
    {
        vec3i block_pos = vec3i(int(pos.x), int(pos.y), int(pos.z));
        block_t *block = get_block_at(block_pos, near);
        if (block)
        {
            if (block->get_blockid() != BlockID::air)
            {
                float blast_resistance = properties(block->id).m_blast_resistance;
                if (blast_resistance < 0)
                    break;
                intensity -= (blast_resistance + 0.3) * 0.3;
                if (intensity <= 0)
                    break;
                if (block->get_blockid() == BlockID::tnt)
                {
                    get_chunk_from_pos(block_pos, false)->entities.push_back(new exploding_block_entity_t(*block, block_pos, rand() % 20 + 10));
                }
                block->set_blockid(BlockID::air);
                block->meta = 0;
                update_block_at(block_pos);
                update_neighbors(block_pos);
            }
            intensity -= 0.225;
            if (intensity <= 0)
                break;
        }
        else
            break;

        pos = pos + dir;
    }
}

inline void explode(vec3f position, float power, chunk_t *near)
{
    vec3f dir;
    block_t *center_block = get_block_at(vec3i(int(position.x), int(position.y), int(position.z)), near);
    power -= (properties(center_block->id).m_blast_resistance + 0.3) * 0.3;
    if (power <= 0)
        return;

    // I suppose the float bits of the position vector are enough to generate a seed random enough
    JavaLCGInit(rand());

    for (int x = -8; x <= 8; x++)
    {
        for (int z = -8; z <= 8; z++)
        {
            for (int y = -8; y <= 8; y += 16)
            {
                dir = vec3f(x, y, z);
                explode_raycast(position, dir, power * (JavaLCGFloat() * 0.6 + 0.7), near);
                dir = vec3f(x, z, y);
                explode_raycast(position, dir, power * (JavaLCGFloat() * 0.6 + 0.7), near);
                dir = vec3f(y, z, x);
                explode_raycast(position, dir, power * (JavaLCGFloat() * 0.6 + 0.7), near);
            }
        }
    }
}
#endif
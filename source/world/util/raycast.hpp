#ifndef _RAYCAST_H_
#define _RAYCAST_H_

#include <cmath>
#include <vector>
#include <math/vec3f.hpp>
#include <math/vec3i.hpp>
#include <math/math_utils.h>
#include <ported/Random.hpp>

#include <world/world.hpp>
#include <world/chunk.hpp>
#include <block/blocks.hpp>

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
inline int checkabove(Vec3i pos, Chunk *chunk = nullptr, World *world = nullptr)
{
    Block *block = nullptr;
    chunk = chunk ? chunk : (world ? world->get_chunk_from_pos(pos) : nullptr);
    if (!chunk)
        return pos.y;
    // Starting from the y coordinate, cast a ray up that stops at the first (partially) opaque block.
    pos.y++;
    while (pos.y < MAX_WORLD_Y && (block = chunk->get_block(pos)) && properties(block->get_blockid()).m_collision == CollisionType::none)
        pos.y++;
    // This should return MAX_WORLD_Y at most which means the ray hit the world height limit
    return pos.y;
}
inline int checkbelow(Vec3i pos, Chunk *chunk = nullptr, World *world = nullptr)
{
    Block *block = nullptr;
    chunk = chunk ? chunk : (world ? world->get_chunk_from_pos(pos) : nullptr);
    if (!chunk)
        return pos.y;
    // Starting from the y coordinate, cast a ray down that stops at the first (partially) opaque block.
    pos.y--;
    while (pos.y > 0 && (block = chunk->get_block(pos)) && properties(block->get_blockid()).m_collision == CollisionType::none)
        pos.y--;
    // This should return 0 at most which means the ray hit the bedrock
    return pos.y;
}
inline int skycast(Vec3i pos, Chunk *chunk = nullptr, World *world = nullptr)
{
    Block *block = nullptr;
    chunk = chunk ? chunk : (world ? world->get_chunk_from_pos(pos) : nullptr);
    if (!chunk)
        return -9999;
    // Starting from world height limit, cast a ray down that stops at the first (partially) opaque block.
    pos.y = MAX_WORLD_Y;
    while (pos.y > 0 && (block = chunk->get_block(pos)) && !get_block_opacity(block->get_blockid()))
        pos.y--;
    // If the cast went out of the world bounds, tell it to the caller by returning -9999
    if (!block)
        return -9999;
    return pos.y;
}

// Highly optimized (and unsafe) version of skycast that only checks for sky light.
inline int lightcast(Vec3i pos, Chunk *chunk = nullptr)
{
    Block *block = nullptr;

    // Starting from world height limit, cast a ray down that stops at the first block with sky light < 15.
    for (pos.y = MAX_WORLD_Y; pos.y > 0; pos.y--)
    {
        block = chunk->get_block(pos);
        if (!block || block->sky_light < 15)
            break;
    }
    return pos.y + 1;
}

inline bool raycast(
    Vec3f origin,
    Vec3f direction,
    float dst,
    Vec3i *output,
    Vec3i *output_face, World *world)
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
    Vec3i face = Vec3i(0, 0, 0);

    // Avoids an infinite loop.
    if (dx == 0 && dy == 0 && dz == 0)
        return false;

    // Rescale from units of 1 cube-edge to units of 'direction' so we can
    // compare with 't'.
    float radius = dst * Q_rsqrt(dx * dx + dy * dy + dz * dz);
    // float radius = dst;
    //  Deal with world bounds or their absence.
    Vec3i maxvec = Vec3i(int(origin.x) + (dst + 1), int(origin.y) + (dst + 1), int(origin.z) + (dst + 1));
    Vec3i minvec = Vec3i(int(origin.x) - (dst + 1), int(origin.y) - (dst + 1), int(origin.z) - (dst + 1));

    while (true)
    {
        if (!(x < minvec.x || y < minvec.y || z < minvec.z || x >= maxvec.x || y >= maxvec.y || z >= maxvec.z))
        {
            Vec3i block_pos = Vec3i(int(x), int(y), int(z));
            Block *block = world->get_block_at(block_pos);
            if (block)
            {
                BlockID blockid = block->get_blockid();
                if (blockid != BlockID::air && !is_fluid(blockid))
                {
                    if (output)
                        *output = Vec3i(int(x), int(y), int(z));
                    if (world->get_block_at(block_pos + face) && output_face)
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
    Vec3f origin,
    Vec3f direction,
    float dst,
    Vec3i *output,
    Vec3i *output_face, World *world)
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
    Vec3i face = Vec3i(0, 0, 0);

    // Avoids an infinite loop.
    if (dx == 0 && dy == 0 && dz == 0)
        return false;

    // Rescale from units of 1 cube-edge to units of 'direction' so we can
    // compare with 't'.
    float radius = dst * Q_rsqrt(dx * dx + dy * dy + dz * dz);
    //  Deal with world bounds or their absence.
    Vec3i maxvec = Vec3i(int(origin.x) + (dst + 1), int(origin.y) + (dst + 1), int(origin.z) + (dst + 1));
    Vec3i minvec = Vec3i(int(origin.x) - (dst + 1), int(origin.y) - (dst + 1), int(origin.z) - (dst + 1));

    while (true)
    {
        if (!(x < minvec.x || y < minvec.y || z < minvec.z || x >= maxvec.x || y >= maxvec.y || z >= maxvec.z))
        {
            Vec3i block_pos = Vec3i(int(x), int(y), int(z));
            Block *block = world->get_block_at(block_pos);
            if (block)
            {
                BlockID blockid = block->get_blockid();
                if (blockid == BlockID::air || is_fluid(blockid))
                {
                    if (output)
                        *output = Vec3i(int(x), int(y), int(z));
                    if (world->get_block_at(block_pos + face) && output_face)
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
    Vec3f origin,
    Vec3f direction,
    float dst,
    Vec3i *output,
    Vec3i *output_face, const AABB &aabb)
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
            *output_face = Vec3i(-1, 0, 0);
        else if (t == t2)
            *output_face = Vec3i(1, 0, 0);
        else if (t == t3)
            *output_face = Vec3i(0, -1, 0);
        else if (t == t4)
            *output_face = Vec3i(0, 1, 0);
        else if (t == t5)
            *output_face = Vec3i(0, 0, -1);
        else if (t == t6)
            *output_face = Vec3i(0, 0, 1);
    }
    return true;
}

inline bool raycast_precise(
    Vec3f origin,
    Vec3f direction,
    float dst,
    Vec3i *output,
    Vec3i *output_face,
    AABB &out_aabb, World *world)
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
    Vec3i face = Vec3i(0, 0, 0);

    // Avoids an infinite loop.
    if (dx == 0 && dy == 0 && dz == 0)
        return false;

    // Rescale from units of 1 cube-edge to units of 'direction' so we can
    // compare with 't'.
    float radius = dst * Q_rsqrt(dx * dx + dy * dy + dz * dz);
    // float radius = dst;
    //  Deal with world bounds or their absence.
    Vec3i maxvec = Vec3i(int(origin.x) + (dst + 1), int(origin.y) + (dst + 1), int(origin.z) + (dst + 1));
    Vec3i minvec = Vec3i(int(origin.x) - (dst + 1), int(origin.y) - (dst + 1), int(origin.z) - (dst + 1));

    std::vector<AABB> aabbs;

    while (true)
    {
        if (!(x < minvec.x || y < minvec.y || z < minvec.z || x >= maxvec.x || y >= maxvec.y || z >= maxvec.z))
        {
            Vec3i block_pos = Vec3i(x, y, z);
            Block *block = world->get_block_at(block_pos);
            if (block)
            {
                BlockID blockid = block->get_blockid();
                if (blockid != BlockID::air && !is_fluid(blockid))
                {
                    AABB aabb;
                    aabb.min = Vec3f(block_pos.x, block_pos.y, block_pos.z);
                    aabb.max = aabb.min + Vec3f(1, 1, 1);
                    block->get_aabb(block_pos, aabb, aabbs);
                    if (properties(block->id).m_render_type == RenderType::full)
                    {
                        if (output)
                            *output = block_pos;
                        if (world->get_block_at(block_pos + face) && output_face)
                            *output_face = face;

                        out_aabb = aabb;
                        return true;
                    }
                    else
                    {
                        for (AABB m_aabb : aabbs)
                        {
                            if (raycast_aabb(origin, direction, dst, output, output_face, m_aabb))
                            {
                                *output = block_pos;

                                // Calculate bounding volume for all AABB's within
                                Vec3f b_min = Vec3f(std::numeric_limits<vfloat_t>::max());
                                Vec3f b_max = Vec3f(std::numeric_limits<vfloat_t>::lowest());
                                for (AABB &bounds : aabbs)
                                {
                                    b_min.x = std::min(bounds.min.x, b_min.x);
                                    b_min.y = std::min(bounds.min.y, b_min.y);
                                    b_min.z = std::min(bounds.min.z, b_min.z);

                                    b_max.x = std::max(bounds.max.x, b_max.x);
                                    b_max.y = std::max(bounds.max.y, b_max.y);
                                    b_max.z = std::max(bounds.max.z, b_max.z);
                                }

                                out_aabb.min = b_min;
                                out_aabb.max = b_max;

                                return true;
                            }
                        }
                    }
                    aabbs.clear();
                }
            }
            else
            {
                aabbs.clear();
                return false;
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
inline void explode_raycast(Vec3f origin, Vec3f direction, float intensity, World *world)
{
    // Avoids an infinite loop.
    if (direction.sqr_magnitude() < 0.001)
        return;

    // Rescale from units of 1 cube-edge to units of 'direction' so we can
    // compare with 't'.
    float radius = 0.3f * Q_rsqrt(direction.sqr_magnitude());

    Vec3f pos = origin;
    Vec3f dir = direction * radius;
    while (true)
    {
        Vec3i block_pos = Vec3i(int(pos.x), int(pos.y), int(pos.z));
        Block *block = world->get_block_at(block_pos);
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
                    world->add_entity(new EntityExplosiveBlock(*block, block_pos, rand() % 20 + 10));
                }
                Block old_block = *block;
                world->destroy_block(block_pos, &old_block);
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

inline void explode(Vec3f position, float power, World *world)
{
    Vec3f dir;
    Block *center_block = world->get_block_at(Vec3i(int(position.x), int(position.y), int(position.z)));
    power -= (properties(center_block->id).m_blast_resistance + 0.3) * 0.3;
    if (power <= 0)
        return;

    // I suppose the float bits of the position vector are enough to generate a seed random enough
    javaport::Random rng;

    for (int x = -8; x <= 8; x++)
    {
        for (int z = -8; z <= 8; z++)
        {
            for (int y = -8; y <= 8; y += 16)
            {
                dir = Vec3f(x, y, z);
                explode_raycast(position, dir, power * (rng.nextFloat() * 0.6 + 0.7), world);
                dir = Vec3f(x, z, y);
                explode_raycast(position, dir, power * (rng.nextFloat() * 0.6 + 0.7), world);
                dir = Vec3f(y, z, x);
                explode_raycast(position, dir, power * (rng.nextFloat() * 0.6 + 0.7), world);
            }
        }
    }
}
#endif
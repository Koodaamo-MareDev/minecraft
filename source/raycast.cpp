#include <cmath>
#include "vec3d.hpp"
#include "vec3i.hpp"
#include "raycast.hpp"
#include "chunk_new.hpp"
#include "blocks.hpp"
template <typename T>
int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

double mod(double val, double modulus)
{
    return std::fmod((std::fmod(val, modulus) + modulus), modulus);
}

double intbound(double s, double ds)
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
int checkabove(vec3i pos, chunk_t *chunk)
{
    block_t *block = nullptr;
    chunk = chunk ? chunk : get_chunk_from_pos(pos, false, false);
    if (!chunk)
        return pos.y;
    // Starting from the y coordinate, cast a ray up that stops at the first (partially) opaque block.
    pos.y++;
    while (pos.y < 255 && (block = chunk->get_block(pos)) && !get_block_opacity(block->get_blockid()))
        pos.y++;
    // This should return 255 at most which means the ray hit the world height limit
    return pos.y;
}
int skycast(vec3i pos, chunk_t *chunk)
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
bool raycast(
    vec3d origin,
    vec3d direction,
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
    double x = std::ceil(origin.x);
    double y = std::ceil(origin.y);
    double z = std::ceil(origin.z);

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
    float radius = dst / std::sqrt(dx * dx + dy * dy + dz * dz);
    // float radius = dst;
    //  Deal with world bounds or their absence.
    vec3i maxvec = vec3i(int(origin.x) + 10, int(origin.y) + 10, int(origin.z) + 10);
    vec3i minvec = vec3i(int(origin.x) - 10, int(origin.y) - 10, int(origin.z) - 10);

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
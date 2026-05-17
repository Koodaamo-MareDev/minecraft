#pragma once

#include <cstdint>

enum class BlockSoundType : uint8_t
{
    none,
    dirt,
    grass,
    sand,
    wood,
    stone,
    glass,
    metal,
    cloth,
};

enum class BlockRenderType : uint8_t
{
    full,
    full_special,
    cross,
    flat_ground,
    slab,
    special,
    fluid,
};

enum class BlockCollisionType : uint8_t
{
    none,
    solid,
    fluid,
    slab,
};

enum class BlockFace : uint8_t
{
    NX,
    PX,
    NY,
    PY,
    NZ,
    PZ,
};

inline static uint8_t operator+(BlockFace a)
{
    return static_cast<uint8_t>(a);
}

static const Vec3i block_face[6] = {
    Vec3i{-1, 0, 0},
    Vec3i{+1, 0, 0},
    Vec3i{0, -1, 0},
    Vec3i{0, +1, 0},
    Vec3i{0, 0, -1},
    Vec3i{0, 0, +1},
};
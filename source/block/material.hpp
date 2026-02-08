#ifndef MATERIAL_HPP
#define MATERIAL_HPP

#include <cstdint>

struct Material
{
    bool is_liquid;
    bool is_solid;
    bool can_grass;
    bool can_burn;
};

enum class Materials : uint8_t
{
    AIR,
    GROUND,
    WOOD,
    ROCK,
    IRON,
    WATER,
    LAVA,
    LEAVES,
    PLANTS,
    SPONGE,
    CLOTH,
    FIRE,
    SAND,
    CIRCUITS,
    GLASS,
    TNT,
    UNKNOWN,
    ICE,
    SNOW_LAYER,
    SNOW,
    CACTUS,
    CLAY,
    PUMPKIN,
    PORTAL,
    CAKE
};

const Material &get_material(Materials material);

#endif
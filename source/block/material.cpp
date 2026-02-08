#include "material.hpp"

#include <map>

static std::map<Materials, Material> materials =
    {{Materials::AIR, Material{false, false, false, false}},
     {Materials::GROUND, Material{false, true, true, false}},
     {Materials::WOOD, Material{false, true, true, true}},
     {Materials::ROCK, Material{false, true, true, false}},
     {Materials::IRON, Material{false, true, true, false}},
     {Materials::WATER, Material{true, false, false, false}},
     {Materials::LAVA, Material{true, false, false, false}},
     {Materials::LEAVES, Material{false, true, true, true}},
     {Materials::PLANTS, Material{false, false, true, false}},
     {Materials::SPONGE, Material{false, true, true, false}},
     {Materials::CLOTH, Material{false, true, true, true}},
     {Materials::FIRE, Material{false, false, false, false}},
     {Materials::SAND, Material{false, true, true, false}},
     {Materials::CIRCUITS, Material{false, true, true, false}},
     {Materials::GLASS, Material{false, false, false, false}},
     {Materials::TNT, Material{false, true, true, true}},
     {Materials::UNKNOWN, Material{false, false, false, false}},
     {Materials::ICE, Material{false, true, true, false}},
     {Materials::SNOW_LAYER, Material{false, true, true, false}},
     {Materials::SNOW, Material{false, true, true, false}},
     {Materials::CACTUS, Material{false, true, true, false}},
     {Materials::CLAY, Material{false, true, true, false}},
     {Materials::PUMPKIN, Material{false, true, true, false}},
     {Materials::PORTAL, Material{false, false, false, false}},
     {Materials::CAKE, Material{false, true, true, false}}};

const Material &get_material(Materials material)
{
    return materials[material];
}
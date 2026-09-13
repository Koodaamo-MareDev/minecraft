#pragma once
#include <string>
#include <cstdint>
#include <ogc/gx.h>
#include <block/block_id.hpp>

class Biome
{
public:
    std::string name = "Sky";
    GXColor sky_color{0x00, 0x00, 0x00, 0xFF};
    GXColor grass_color{0x4E, 0xE0, 0x31, 0xFF};
    BlockID top_block = BlockID::grass;
    BlockID filler_block = BlockID::dirt;

    Biome(const GXColor &color, const GXColor &grass_color = GXColor{0x4E, 0xE0, 0x31, 0xFF}) : sky_color(color), grass_color(grass_color) {};

    static void generate_lookup();
    static const Biome *lookup(float temperature, float humidity);
    static const Biome *calculate(float temperature, float humidity);
};
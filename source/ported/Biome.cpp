#include "Biome.hpp"

Biome rainforest(GXColor{0x08, 0xFA, 0x36, 0xFF});
Biome swampland(GXColor{0x07, 0xF9, 0xB2, 0xFF});
Biome seasonal_forest(GXColor{0x9B, 0xE0, 0x23, 0xFF});
Biome forest(GXColor{0x05, 0x66, 0x21, 0xFF});
Biome savanna(GXColor{0xD9, 0xE0, 0x23, 0xFF});
Biome shrubland(GXColor{0xA1, 0xAD, 0x20, 0xFF});
Biome taiga(GXColor{0x2E, 0xB1, 0x53, 0xFF});
Biome desert(GXColor{0xFA, 0x94, 0x18, 0xFF});
Biome plains(GXColor{0xFF, 0xD9, 0x10, 0xFF});
Biome tundra(GXColor{0x57, 0xEB, 0xF9, 0xFF});

static const Biome *biome_lut[4096];

void Biome::generate_lookup()
{
    for (int t = 0; t < 64; t++)
        for (int h = 0; h < 64; h++)
            biome_lut[t + h * 64] = calculate(static_cast<float>(t) / 63.0f, static_cast<float>(h) / 63.0f);
    desert.top_block = desert.filler_block = BlockID::sand;
}

const Biome *Biome::lookup(float temperature, float humidity)
{
    static bool has_generated_lookup = false;
    if (!has_generated_lookup)
    {
        generate_lookup();
        has_generated_lookup = true;
    }
    int t = static_cast<int>(temperature * 63.0f);
    int h = static_cast<int>(humidity * 63.0f);
    return biome_lut[t + h * 64];
}

const Biome *Biome::calculate(float temperature, float humidity)
{
    if (temperature < 0.1f)
        return &tundra;

    humidity *= temperature;

    if (humidity < 0.2f)
    {
        if (temperature < 0.5f)
            return &tundra;
        if (temperature < 0.95f)
            return &savanna;
        return &desert;
    }
    if (humidity > 0.5f && temperature < 0.7f)
        return &swampland;
    if (temperature < 0.5f)
        return &taiga;
    if (temperature < 0.97f)
    {
        if (humidity < 0.35f)
            return &shrubland;
        return &forest;
    }
    if (humidity < 0.45f)
        return &plains;
    if (humidity < 0.9f)
        return &seasonal_forest;
    return &forest;
}
#include "MapGenSurface.hpp"
#include "NoiseSynthesizer.hpp"
#include "../raycast.hpp"

void javaport::MapGenSurface::populate(int32_t chunkX, int32_t chunkZ, int32_t x, int32_t z, BlockID *out_ids)
{
    for (int32_t i = 16 * 16 * WORLD_HEIGHT - 1; i >= 16 * 16 * 64; i--)
    {
        if (out_ids[i] == BlockID::air && out_ids[i - 256] == BlockID::stone)
        {
            out_ids[i - 256] = BlockID::grass;
            for (int j = 2; j < 4; j++)
            {
                if (out_ids[i - (j << 8)] == BlockID::stone)
                {
                    out_ids[i - (j << 8)] = BlockID::dirt;
                    continue;
                }
                break;
            }
        }
    }

    float temperature_noise[16 * 16];
    noiser.GetNoiseSet(Vec3f(x * 16 - 193.514f, 0, z * 16 + 37.314f), Vec3i(16, 1, 16), 131.134f, 2, temperature_noise);

    for (int32_t i = 0; i < 16 * 16; i++)
    {
        float noise_sample = temperature_noise[i];

        // Check for cold biome
        if (noise_sample > 0.7f)
        {
            // Cover grass with snow
            for (int y = MAX_WORLD_Y - 1; y >= 63; y--)
            {
                int index = (y << 8) | i;
                if (out_ids[index] == BlockID::grass && out_ids[index + 256] == BlockID::air)
                {
                    out_ids[index + 256] = BlockID::snow_layer;
                    break;
                }
            }

            // Cover water with ice
            if (out_ids[i | (63 << 8)] == BlockID::water)
            {
                out_ids[i | (63 << 8)] = BlockID::ice;
            }
        }
        
        // Check for warm biome
        if (noise_sample < 0.37f)
        {
            // Coat with sand
            for (int32_t y = 60; y < WORLD_HEIGHT; y++)
            {
                int index = (y << 8) | i;
                if (out_ids[index] == BlockID::grass)
                {
                    out_ids[index] = BlockID::sand;
                    int j;
                    for (j = 1; j < 4; j++, index -= 256)
                    {
                        if (out_ids[index] == BlockID::dirt)
                        {
                            out_ids[index] = BlockID::sand;
                        }
                    }
                    for (; j < 6; j++, index -= 256)
                    {
                        if (out_ids[index] == BlockID::stone)
                        {
                            out_ids[index] = BlockID::sandstone;
                        }
                    }
                }
            }
        }
    }
}
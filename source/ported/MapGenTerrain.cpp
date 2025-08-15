#include "MapGenTerrain.hpp"
#include "NoiseSynthesizer.hpp"
namespace javaport
{
    void MapGenTerrain::populate(int32_t, int32_t, int32_t chunkX, int32_t chunkZ, BlockID *out_ids)
    {
        constexpr int32_t coarse_width = 4;
        constexpr int32_t coarse_height = 16;
        constexpr int32_t sea_level = 64;
        constexpr int32_t x_max = coarse_width + 1;
        constexpr int32_t y_max = coarse_height + 1;
        constexpr int32_t z_max = coarse_width + 1;
        constexpr int32_t cell_stride = 16;

        constexpr float x_scale = 0.25f;
        constexpr float y_scale = 0.125f;
        constexpr float z_scale = 0.25f;

        float biome_noise_2d[64];
        float noise_2d[256];
        float noise_3d[x_max * y_max * z_max];
        noiser.GetNoiseSet(Vec3f(chunkX * 16, 0, chunkZ * 16), Vec3i(16, 1, 16), 24.0f, 3, noise_2d);
        noiser.GetNoiseSet(Vec3f(chunkX * 8 - 193.514f, 0, chunkZ * 8 + 93.314f), Vec3i(8, 1, 8), 135.24f, 1, biome_noise_2d);
        noiser.GetNoiseSet(Vec3f(chunkX * coarse_width, 0, chunkZ * coarse_width), Vec3i(x_max, y_max, z_max), 4.0f, 1, noise_3d);

        for (int32_t i = x_max * y_max * z_max - 1; i >= 0; --i)
        {
            noise_3d[i] = noise_3d[i] * 2.0f - 1.0f;
        }

        for (int32_t z = 0, i = 0; z < 16; ++z)
        {
            for (int32_t x = 0; x < 16; ++x, ++i)
            {
                noise_2d[i] *= std::pow(biome_noise_2d[((z >> 1) << 3) | (x >> 1)] * 1.5f, 2.0f);
            }
        }

        for (int32_t x_coarse = 0; x_coarse < coarse_width; ++x_coarse)
        {
            for (int32_t z_coarse = 0; z_coarse < coarse_width; ++z_coarse)
            {
                for (int32_t y_coarse = 0; y_coarse < coarse_height; ++y_coarse)
                {

                    float y1 = noise_3d[(y_coarse * z_max + z_coarse) * x_max + x_coarse];
                    float y2 = noise_3d[(y_coarse * z_max + z_coarse + 1) * x_max + x_coarse];
                    float y3 = noise_3d[(y_coarse * z_max + z_coarse) * x_max + x_coarse + 1];
                    float y4 = noise_3d[(y_coarse * z_max + z_coarse + 1) * x_max + x_coarse + 1];

                    float y1_increment = (noise_3d[((y_coarse + 1) * z_max + z_coarse) * x_max + x_coarse] - y1) * y_scale;
                    float y2_increment = (noise_3d[((y_coarse + 1) * z_max + z_coarse + 1) * x_max + x_coarse] - y2) * y_scale;
                    float y3_increment = (noise_3d[((y_coarse + 1) * z_max + z_coarse) * x_max + x_coarse + 1] - y3) * y_scale;
                    float y4_increment = (noise_3d[((y_coarse + 1) * z_max + z_coarse + 1) * x_max + x_coarse + 1] - y4) * y_scale;

                    for (int32_t y_fine = 0; y_fine < 8; ++y_fine)
                    {
                        float x1 = y1;
                        float x2 = y2;
                        float x1_increment = (y3 - y1) * x_scale;
                        float x2_increment = (y4 - y2) * x_scale;

                        int y = ((y_coarse << 3) | y_fine);

                        for (int32_t x_fine = 0; x_fine < 4; ++x_fine)
                        {
                            int32_t index = (x_fine + (x_coarse << 2)) | (z_coarse << 6) | (y << 8);
                            float z1 = x1;
                            float z1_increment = (x2 - x1) * z_scale;

                            for (int32_t z_fine = 0; z_fine < 4; ++z_fine)
                            {
                                float temperature = 0.6; // temperatures[(((x_coarse << 2) + x_fine) << 4) + (((z_coarse << 2) + z_fine) << 4)];
                                BlockID block_id = BlockID::air;
                                if (y < sea_level)
                                {
                                    if (temperature < 0.5 && y >= sea_level - 1)
                                    {
                                        block_id = BlockID::ice;
                                    }
                                    else
                                    {
                                        block_id = BlockID::water;
                                    }
                                }

                                float noise_value = noise_2d[index & 0xFF];
                                float noise_threshold = lerpf(0.0f, 48.0f, std::clamp((noise_value * 4.0f) * noise_value * noise_value, 0.0f, 1.0f));
                                float noise_height = 56 + noise_value * noise_threshold;
                                float stone_chance = noise_value - 0.5f;
                                if (stone_chance > 0.0f)
                                    stone_chance *= 1.5f;
                                else if (stone_chance < -0.25f)
                                    stone_chance *= 0.35f;
                                if (y < noise_height && (y < sea_level || z1 < stone_chance))
                                {
                                    block_id = BlockID::stone;
                                }

                                out_ids[index] = block_id;
                                index += cell_stride;
                                z1 += z1_increment;
                            }

                            x1 += x1_increment;
                            x2 += x2_increment;
                        }

                        y1 += y1_increment;
                        y2 += y2_increment;
                        y3 += y3_increment;
                        y4 += y4_increment;
                    }
                }
            }
        }
        noiser.GetNoiseSet(Vec3f(chunkX * 16 - 7193.514f, 0, chunkZ * 16 + 937.314f), Vec3i(16, 1, 16), 21.121f, 2, noise_2d);
        for (int32_t z = 0, i = 0; z < 16; ++z)
        {
            for (int32_t x = 0; x < 16; ++x, ++i)
            {
                noise_2d[i] += std::pow(biome_noise_2d[((z >> 1) << 3) | (x >> 1)] - 0.35f, 2.0f);
                for (int32_t y = 54 + std::min(int32_t(noise_2d[i] * 20), 127); y > 0; y--)
                {
                    out_ids[i | (y << 8)] = BlockID::stone;
                }
            }
        }
    }

}
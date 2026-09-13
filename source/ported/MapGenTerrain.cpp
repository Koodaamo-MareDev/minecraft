#include "MapGenTerrain.hpp"

namespace javaport
{
    void MapGenTerrain::populate(int32_t, int32_t, int32_t chunkX, int32_t chunkZ, BlockID *out_ids)
    {
        this->rng = javaport::Random(chunkX * 341873128712L + chunkZ * 132897987541L);
        constexpr int32_t coarse_width = 4;
        constexpr int32_t coarse_height = 16;
        constexpr int32_t sea_level = 64;
        constexpr int32_t x_size = coarse_width + 1;
        constexpr int32_t y_size = coarse_height + 1;
        constexpr int32_t z_size = coarse_width + 1;
        constexpr int32_t cell_stride = 16;

        constexpr float x_scale = 0.25f;
        constexpr float y_scale = 0.125f;
        constexpr float z_scale = 0.25f;

        const size_t noise_3d_len = x_size * y_size * z_size;

        generate_biome_data(chunkX * 16, chunkZ * 16, 16, 16);

        float noise_3d[noise_3d_len];
        generate_density_field(noise_3d, chunkX * coarse_width, 0, chunkZ * coarse_width, x_size, y_size, z_size);

        for (int32_t x_coarse = 0; x_coarse < coarse_width; ++x_coarse)
        {
            for (int32_t z_coarse = 0; z_coarse < coarse_width; ++z_coarse)
            {
                for (int32_t y_coarse = 0; y_coarse < coarse_height; ++y_coarse)
                {
                    float y1 = noise_3d[(x_coarse * z_size + z_coarse) * y_size + y_coarse];
                    float y2 = noise_3d[(x_coarse * z_size + z_coarse + 1) * y_size + y_coarse];
                    float y3 = noise_3d[((x_coarse + 1) * z_size + z_coarse) * y_size + y_coarse];
                    float y4 = noise_3d[((x_coarse + 1) * z_size + z_coarse + 1) * y_size + y_coarse];

                    float y1_increment = (noise_3d[(x_coarse * z_size + z_coarse) * y_size + y_coarse + 1] - y1) * y_scale;
                    float y2_increment = (noise_3d[(x_coarse * z_size + z_coarse + 1) * y_size + y_coarse + 1] - y2) * y_scale;
                    float y3_increment = (noise_3d[((x_coarse + 1) * z_size + z_coarse) * y_size + y_coarse + 1] - y3) * y_scale;
                    float y4_increment = (noise_3d[((x_coarse + 1) * z_size + z_coarse + 1) * y_size + y_coarse + 1] - y4) * y_scale;

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
                                float t = this->temperature[(z_fine + (z_coarse << 2)) | (((x_coarse << 2) + x_fine) << 4)];
                                BlockID block_id = BlockID::air;
                                if (y < sea_level)
                                {
                                    if (t < 0.5f && y >= sea_level - 1)
                                    {
                                        block_id = BlockID::ice;
                                    }
                                    else
                                    {
                                        block_id = BlockID::water;
                                    }
                                }

                                if (z1 > 0.0f)
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

        biome_coat(chunkX, chunkZ, out_ids, biome_lookup.data());
    }

    void MapGenTerrain::biome_coat(int x, int z, BlockID *out_ids, const Biome **biomes)
    {
        constexpr int32_t sea_level = 64;
        constexpr float frequency = (1.0f / 32.0f);

        std::vector<float> sand_noise = std::vector<float>(256, 1.0f);
        std::vector<float> gravel_noise = std::vector<float>(256, 1.0f);
        std::vector<float> stone_noise = std::vector<float>(256, 1.0f);

        this->sand_noise_generator.sample_batch(sand_noise.data(), x << 4, z << 4, 0.0f, 16, 16, 1, frequency, frequency, 1.0f);
        this->sand_noise_generator.sample_batch(gravel_noise.data(), x << 4, 109.0134f, z << 4, 16, 1, 16, frequency, 1.0f, frequency);
        this->stone_noise_generator.sample_batch(stone_noise.data(), x << 4, z << 4, 0.0f, 16, 16, 1, frequency * 2.0f, frequency * 2.0f, frequency * 2.0f);
        for (int i = 0; i < 16; i++)
            for (int j = 0; j < 16; j++)
            {
                int index = (j << 4) | i;
                const Biome *b = biomes[index];
                const bool sand = sand_noise[index] + rng.nextDouble() * 0.2f > 0.0f;
                const bool gravel = gravel_noise[index] + rng.nextDouble() * 0.2f > 3.0f;
                const int stone_dist = static_cast<int>(stone_noise[index] / 3.0f + 3.0f + this->rng.nextDouble() * 0.25f);

                BlockID top = b->top_block;
                BlockID fill = b->filler_block;
                int y_off = -1;
                for (int y = MAX_WORLD_Y; y >= 0; y--)
                {
                    BlockID &target = out_ids[(i << 4) | j | (y << 8)];
                    if (y <= rng.nextInt(5))
                    {
                        target = BlockID::bedrock;
                        continue;
                    }

                    if (target == BlockID::air)
                    {
                        y_off = -1;
                    }
                    else if (target == BlockID::stone)
                    {
                        if (y_off == -1)
                        {
                            if (stone_dist <= 0)
                            {
                                top = BlockID::air;
                                fill = BlockID::stone;
                            }
                            else if (y >= sea_level - 4 && y <= sea_level + 1)
                            {
                                top = b->top_block;
                                fill = b->filler_block;
                                if (gravel)
                                {
                                    top = BlockID::air;
                                    fill = BlockID::gravel;
                                }
                                if (sand)
                                {
                                    top = BlockID::sand;
                                    fill = BlockID::sand;
                                }
                            }
                            if (y < sea_level && top == BlockID::air)
                                top = BlockID::water;

                            y_off = stone_dist;
                            if (y >= sea_level - 1)
                                target = top;
                            else
                                target = fill;
                        }
                        else if (y_off > 0)
                        {
                            y_off--;
                            target = fill;
                            if (y_off == 0 && fill == BlockID::sand)
                            {
                                y_off = rng.nextInt(4);
                                fill = BlockID::sandstone;
                            }
                        }
                    }
                }
            }
    }

    void MapGenTerrain::generate_biome_data(int x, int z, int x_size, int z_size)
    {
        temperature_noise_generator.sample_batch(temperature.data(), x, z, x_size, z_size, 0.025f, 0.025f, 0.25f);
        humidity_noise_generator.sample_batch(humidity.data(), x, z, x_size, z_size, 0.05f, 0.05f, 1.0f / 3.0f);
        biome_noise_generator.sample_batch(biome.data(), x, z, x_size, z_size, 0.25f, 0.25f, 150.0f / 255.0f);
        const int size = x_size * z_size;
        for (int i = 0; i < size; i++)
        {
            float biome_noise = biome[i] * 1.1f + 0.5f;
            float mix_factor = 0.01f;
            float inv_mix_factor = 1.0f - mix_factor;
            float temperature_mixed = (temperature[i] * 0.15f + 0.7f) * inv_mix_factor + biome_noise * mix_factor;
            inv_mix_factor = 0.002f;
            inv_mix_factor = 1.0f - mix_factor;
            float humidity_mixed = (temperature[i] * 0.15f + 0.5f) * inv_mix_factor + biome_noise * mix_factor;
            float result = 1.0f - (1.0f - temperature_mixed) * (1.0f - temperature_mixed);
            temperature[i] = std::clamp(result, 0.0f, 1.0f);
            humidity[i] = std::clamp(humidity_mixed, 0.0f, 1.0f);
            biome_lookup[i] = Biome::lookup(temperature[i], humidity[i]);
        }
    }

    void MapGenTerrain::generate_density_field(float *density, int x, int y, int z, int size_x, int size_y, int size_z)
    {
        const int total_size = size_x * size_y * size_z;

        std::vector<float> depth_noise;
        std::vector<float> offset_noise;
        std::vector<float> selector_noise;
        std::vector<float> low_noise;
        std::vector<float> high_noise;

        low_noise.resize(total_size);
        high_noise.resize(total_size);
        selector_noise.resize(total_size);
        depth_noise.resize(total_size);
        offset_noise.resize(total_size);

        // Kept separate for clarity
        constexpr float noise_scale_horizontal = 684.412f;
        constexpr float noise_scale_vertical = 684.412f;

        // Convert once as we pass these as floats
        const float xf = static_cast<float>(x);
        const float yf = static_cast<float>(y);
        const float zf = static_cast<float>(z);

        low_noise_generator.sample_batch2d(
            low_noise.data(),
            x,
            z,
            size_x,
            size_z,
            1.121f,
            1.121f,
            0.5f);

        high_noise_generator.sample_batch2d(
            high_noise.data(),
            x,
            z,
            size_x,
            size_z,
            200.0f,
            200.0f,
            0.5f);

        selector_noise_generator.sample_batch(
            selector_noise.data(),
            xf,
            yf,
            zf,
            size_x,
            size_y,
            size_z,
            noise_scale_horizontal / 80.0f,
            noise_scale_vertical / 160.0f,
            noise_scale_horizontal / 80.0f);

        depth_noise_generator.sample_batch(
            depth_noise.data(),
            xf,
            yf,
            zf,
            size_x,
            size_y,
            size_z,
            noise_scale_horizontal,
            noise_scale_vertical,
            noise_scale_horizontal);

        terrain_offset_generator.sample_batch(
            offset_noise.data(),
            x,
            y,
            z,
            size_x,
            size_y,
            size_z,
            noise_scale_horizontal,
            noise_scale_vertical,
            noise_scale_horizontal);
        int density_index = 0;
        int climate_index = 0;

        const int climate_step = 16 / size_x;

        for (int x_index = 0; x_index < size_x; ++x_index)
        {
            const int climate_x = x_index * climate_step + (climate_step >> 1);

            for (int z_index = 0; z_index < size_z; ++z_index)
            {
                const int climate_z = z_index * climate_step + (climate_step >> 1);

                const float local_temperature = temperature[climate_x * 16 + climate_z];

                const float temperature_humidity = humidity[climate_x * 16 + climate_z] * local_temperature;

                // Convert temperature/humidity into a terrain influence.
                float terrain_influence = 1.0f - temperature_humidity;
                terrain_influence *= terrain_influence;
                terrain_influence *= terrain_influence;
                terrain_influence = 1.0f - terrain_influence;

                float terrain_scale = (low_noise[climate_index] + 256.0f) / 512.0f;

                terrain_scale *= terrain_influence;

                if (terrain_scale > 1.0f)
                    terrain_scale = 1.0f;

                float terrain_offset = high_noise[climate_index] / 8000.0f;

                if (terrain_offset < 0.0f)
                    terrain_offset = -terrain_offset * 0.3f;

                terrain_offset = terrain_offset * 3.0f - 2.0f;

                if (terrain_offset < 0.0f)
                {
                    terrain_offset *= 0.5f;

                    if (terrain_offset < -1.0f)
                        terrain_offset = -1.0f;

                    // Original here was:
                    // x /= 2, x /= 1.4
                    terrain_offset *= (1.0f / 2.8f);

                    // This was changed from 0 to 0.5 as the 0.5 is
                    // no longer being added afterwards.
                    // See the 'else' below
                    terrain_scale = 0.5f;
                }
                else
                {
                    if (terrain_offset > 1.0f)
                        terrain_offset = 1.0f;

                    terrain_offset *= 0.125f;

                    // This was optimized by using max and then moving it
                    // inside the else statement instead of after it.
                    terrain_scale = std::max(0.5f, terrain_scale + 0.5f);
                }

                terrain_offset = terrain_offset * static_cast<float>(size_y) * 0.0625f;

                const float terrain_height = static_cast<float>(size_y) * 0.5f + terrain_offset * 4.0f;

                ++climate_index;

                for (int y_index = 0; y_index < size_y; ++y_index)
                {
                    float density_value = 0.0f;

                    float vertical_offset = (static_cast<float>(y_index) - terrain_height) * 12.0f / terrain_scale;

                    if (vertical_offset < 0.0f)
                        vertical_offset *= 4.0f;

                    const float depth = depth_noise[density_index] / 512.0f;

                    const float terrain_offset = offset_noise[density_index] / 512.0f;

                    const float selector = (selector_noise[density_index] * 0.1f + 1.0f) * 0.5f;

                    // Determine noise source
                    if (selector < 0.0f)
                    {
                        // Entirely from depth noise
                        density_value = depth;
                    }
                    else if (selector > 1.0f)
                    {
                        // Entirely from offset noise
                        density_value = terrain_offset;
                    }
                    else
                    {
                        // Anything between the two
                        density_value = depth + (terrain_offset - depth) * selector;
                    }

                    // Remove offset
                    density_value -= vertical_offset;

                    // Fade towards -10
                    if (y_index > size_y - 4)
                    {
                        const float bottom_fade = static_cast<float>(y_index - (size_y - 4)) / 3.0f;
                        density_value = density_value * (1.0f - bottom_fade) - 10.0f * bottom_fade;
                    }

                    density[density_index] = density_value;
                    ++density_index;
                }
            }
        }
    }

}
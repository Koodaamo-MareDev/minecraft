#ifndef MAPGENTERRAIN_H
#define MAPGENTERRAIN_H

#include "MapGenBase.hpp"
#include "PerlinNoise.hpp"
#include "OctaveNoise.hpp"
#include "OctaveNoise2.hpp"
#include "Biome.hpp"
#include <cmath>
#include <algorithm>

namespace javaport
{
    class MapGenTerrain : public MapGenBase
    {
    private:
        std::vector<float> temperature = std::vector<float>(256, 1.0f);
        std::vector<float> humidity = std::vector<float>(256, 1.0f);
        std::vector<float> biome = std::vector<float>(256, 1.0f);
        std::vector<const Biome *> biome_lookup = std::vector<const Biome *>(256, nullptr);

        NoiseGeneratorOctaves depth_noise_generator;
        NoiseGeneratorOctaves terrain_offset_generator;
        NoiseGeneratorOctaves selector_noise_generator;
        NoiseGeneratorOctaves sand_noise_generator;
        NoiseGeneratorOctaves stone_noise_generator;
        NoiseGeneratorOctaves low_noise_generator;
        NoiseGeneratorOctaves high_noise_generator;

        NoiseGeneratorOctaves2 temperature_noise_generator;
        NoiseGeneratorOctaves2 humidity_noise_generator;
        NoiseGeneratorOctaves2 biome_noise_generator;

    public:
        MapGenTerrain(Random rng, int64_t world_seed) : depth_noise_generator(rng, 16),
                                                        terrain_offset_generator(rng, 16),
                                                        selector_noise_generator(rng, 8),
                                                        sand_noise_generator(rng, 4),
                                                        stone_noise_generator(rng, 4),
                                                        low_noise_generator(rng, 10),
                                                        high_noise_generator(rng, 16),
                                                        temperature_noise_generator(javaport::Random(world_seed * 9871), 4),
                                                        humidity_noise_generator(javaport::Random(world_seed * 39811), 4),
                                                        biome_noise_generator(javaport::Random(world_seed * 543321), 2)
        {
            maxDist = 0;
        }
        void populate(int32_t offX, int32_t offZ, int32_t chunkX, int32_t chunkZ, BlockID *out_ids);
        void biome_coat(int x, int z, BlockID *out_ids, const Biome **biomes);
        void generate_biome_data(int x, int z, int size_x, int size_z);
        void generate_density_field(float *density, int x, int y, int z, int size_x, int size_y, int size_z);
    };
}

#endif
#pragma once

#include "NoiseGen2.hpp"
#include <vector>

class NoiseGeneratorOctaves2
{
    std::vector<NoiseGenerator2> generators;

public:
    NoiseGeneratorOctaves2(javaport::Random rng, size_t octaves)
    {
        for (size_t i = 0; i < octaves; i++)
            generators.emplace_back(rng);
    }

    void sample_batch(float *dst, float start_x, float start_z, int x_size, int z_size, float x_scale, float z_scale, float freq_mul, float denom_mul = 0.5f)
    {
        x_scale /= 1.5f;
        z_scale /= 1.5f;
        int len = x_size * z_size;

        for (int i = 0; i < len; ++i)
        {
            dst[i] = 0.0f;
        }

        float freq = 1.0f;
        float denom = 1.0f;

        for (size_t i = 0; i < generators.size(); ++i)
        {
            generators[i].sample_batch(dst, start_x, start_z, x_size, z_size, x_scale * freq, z_scale * freq, 0.55f / denom);
            freq *= freq_mul;
            denom *= denom_mul;
        }
    }
};
#pragma once

#include <vector>
#include "PerlinNoise.hpp"

class NoiseGeneratorOctaves
{
    std::vector<NoiseGeneratorPerlin<float>> generators;

public:
    NoiseGeneratorOctaves(javaport::Random &rng, size_t octaves)
    {
        for (size_t i = 0; i < octaves; i++)
            generators.emplace_back(rng);
    }

    float sample2d(float x, float y)
    {
        float ret = 0.0f;
        float denom = 1.0f;

        for (size_t i = 0; i < generators.size(); ++i)
        {
            ret += generators[i].sample2d(x * denom, y * denom) / denom;
            denom *= 0.5f;
        }

        return ret;
    }

    void sample_batch(float *dst, float start_x, float start_y, float start_z, int x_size, int y_size, int z_size, float x_scale, float y_scale, float z_scale)
    {
        int len = x_size * y_size * z_size;

        for (int i = 0; i < len; ++i)
        {
            dst[i] = 0.0f;
        }

        float denom = 1.0f;

        for (size_t i = 0; i < generators.size(); ++i)
        {
            generators[i].sample_batch(dst, start_x, start_y, start_z, x_size, y_size, z_size, x_scale * denom, y_scale * denom, z_scale * denom, denom);
            denom *= 0.5f;
        }
    }

    void sample_batch2d(float *dst, int start_x, int start_z, int x_size, int z_size, float x_scale, float z_scale, float /* amplitude? */)
    {
        return this->sample_batch(dst, start_x, 10.0f, start_z, x_size, 1, z_size, x_scale, 1.0f, z_scale);
    }
};
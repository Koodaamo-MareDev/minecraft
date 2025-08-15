#ifndef IMPROVED_NOISE_HPP
#define IMPROVED_NOISE_HPP

#include <unistd.h>
#include <cstdint>
#include <cmath>
#include <functional>
#include <math/vec3f.hpp>

#include "Random.hpp"
#include "../perlinnoise_double.h"

// Noise generator that internally uses Ken Perlin's noise algorithm
// Added support for noise sets, octaves, and seeded noise generation by Marcus Erkkilä

class NoiseSynthesizer
{
    float inv_u8[256];

public:
    inline void Init(uint32_t seed)
    {
        srand(seed);
        init_noise();

        // Precompute the inverse of 0-255
        for (int i = 1; i < 256; i++)
            inv_u8[i] = 1.0f / i;
    }

    inline double GetNoise(double x, double z)
    {
        double pos[2] = {x, z};
        return noise2(pos);
    }

    inline double GetNoise(double x, double y, double z)
    {
        double pos[3] = {x, y, z};
        return noise3(pos);
    }
    template <typename T>
    typename std::enable_if<std::is_integral<T>::value, void>::type GetNoiseSet(const Vec3i &pos, const Vec3i &size, float frequency, uint8_t octaves, T *out)
    {
        float amplitude = (1 << (sizeof(T) << 3)) - 1;
        float inv_frequency = 1.0f / frequency;
        float half_amplitude = amplitude * 0.5f;

        if (size.y == 1)
        {
            float _z = pos.z * inv_frequency;
            for (int32_t i = 0; i < size.z; i++, _z += inv_frequency)
            {
                float _x = pos.x * inv_frequency;
                for (int32_t k = 0; k < size.x; k++, _x += inv_frequency)
                {
                    float val = 0;
                    for (uint8_t l = 0, octave_multiplier = 1; l < octaves; l++, octave_multiplier <<= 1)
                    {
                        val += GetNoise(_x * octave_multiplier, _z * octave_multiplier) * inv_u8[octave_multiplier];
                    }
                    *(out++) = T(half_amplitude + (half_amplitude * val));
                }
            }
            return;
        }

        float _y = pos.y * inv_frequency;
        for (int32_t j = 0; j < size.y; j++, _y += inv_frequency)
        {
            float _z = pos.z * inv_frequency;
            for (int32_t i = 0; i < size.z; i++, _z += inv_frequency)
            {
                float _x = pos.x * inv_frequency;
                for (int32_t k = 0; k < size.x; k++, _x += inv_frequency)
                {
                    float val = 0;
                    for (uint8_t l = 0, octave_multiplier = 1; l < octaves; l++, octave_multiplier <<= 1)
                    {
                        val += GetNoise(_x * octave_multiplier, _y * octave_multiplier, _z * octave_multiplier) * inv_u8[octave_multiplier];
                    }
                    *(out++) = T(half_amplitude + (half_amplitude * val));
                }
            }
        }
    }

    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value, void>::type GetNoiseSet(const Vec3f &pos, const Vec3i &size, float frequency, uint8_t octaves, T *out)
    {
        T amplitude = 1.0f;
        T inv_frequency = 1.0f / frequency;
        T half_amplitude = amplitude * 0.5f;
        if (size.y == 1)
        {
            float _z = pos.z * inv_frequency;
            for (int32_t i = 0; i < size.z; i++, _z += inv_frequency)
            {
                float _x = pos.x * inv_frequency;
                for (int32_t k = 0; k < size.x; k++, _x += inv_frequency)
                {
                    float val = 0;
                    for (uint8_t l = 0, octave_multiplier = 1; l < octaves; l++, octave_multiplier <<= 1)
                    {
                        val += GetNoise(_x * octave_multiplier, _z * octave_multiplier) * inv_u8[octave_multiplier];
                    }
                    *(out++) = T(half_amplitude + (half_amplitude * val));
                }
            }
            return;
        }
        T _y = pos.y * inv_frequency;
        for (int32_t j = 0; j < size.y; j++, _y += inv_frequency)
        {
            T _z = pos.z * inv_frequency;
            for (int32_t i = 0; i < size.z; i++, _z += inv_frequency)
            {
                T _x = pos.x * inv_frequency;
                for (int32_t k = 0; k < size.x; k++, _x += inv_frequency)
                {
                    T val = 0;
                    for (uint8_t l = 0, octave_multiplier = 1; l < octaves; l++, octave_multiplier <<= 1)
                    {
                        val += GetNoise(_x * octave_multiplier, _y * octave_multiplier, _z * octave_multiplier) * inv_u8[octave_multiplier];
                    }
                    *(out++) = T(half_amplitude + (half_amplitude * val));
                }
            }
        }
    }
};
#endif
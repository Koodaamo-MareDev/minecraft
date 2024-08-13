#ifndef IMPROVED_NOISE_HPP
#define IMPROVED_NOISE_HPP

#include <unistd.h>
#include <cstdint>
#include <cmath>
#include <functional>
#include "../vec3f.hpp"
#include "../ported/JavaRandom.hpp"
#include "../perlinnoise.h"

// Adapted from Ken Perlin's Improved Noise reference implementation:
// https://cs.nyu.edu/~perlin/noise/
// https://mrl.nyu.edu/~perlin/noise/improved-noise.pdf
// Added support for noise sets, octaves, and seeded noise generation by Marcus Erkkilä

class ImprovedNoise
{
    uint8_t permutation[512];
    float inv_u8[256];
    static bool assign_unless_all(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pad)
    {
        return x <= pad || y <= pad || x >= w - 1 - pad || y >= h - 1 - pad;
    }

    static bool assign_unless_min_x(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pad)
    {
        return x > pad;
    }

    static bool assign_unless_min_y(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pad)
    {
        return y > pad;
    }

    static bool assign_unless_max_x(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pad)
    {
        return x < w - 1 - pad;
    }

    static bool assign_unless_max_y(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pad)
    {
        return y < h - 1 - pad;
    }

public:
    inline void Init(uint32_t seed)
    {
        srandom(seed);

        // Precompute the inverse of 0-255
        for (int i = 1; i < 256; i++)
            inv_u8[i] = 1.0f / i;

/*
        // Ensure the seed is not zero as the XOR Shift PRNG will not work with it
        uint32_t x = seed ? seed : 1;

        // Uniformly shuffle the permutation vector using the seed
        for (int i = 0; i < 256; i++)
            permutation[i] = i;

        // Shuffle the permutation vector using the XOR Shift PRNG
        for (int i = 0; i < 256; i++)
        {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            std::swap(permutation[x & 0xFF], permutation[(~x) & 0xFF]);
        }

        // Duplicate the permutation vector for faster indexing
        for (int i = 0; i < 256; i++)
            permutation[256 + i] = permutation[i];
*/
    }
    inline float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    inline float lerp(float t, float a, float b) { return a + t * (b - a); }
    inline float grad(int hash, float x, float y, float z)
    {
        int h = hash & 15;       // Convert low 4 bits of hash code into 12 simple
        float u = h < 8 ? x : y; // gradient directions, and compute dot product.
        float v = h < 4 ? y : h == 12 || h == 14 ? x
                                                 : z; // Fix repeats at h = 12 to 15
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }

    inline float GetNoise(float x, float z)
    {
        float pos[2] = {x, z};
        return noise2(pos);
    }

    inline float GetNoise(float x, float y, float z)
    {
        float pos[3] = {x, y, z};
        return noise3(pos);
/*
#define FASTFLOOR(x) (((x) > 0) ? ((int)x) : ((int)x - 1))
        int X = FASTFLOOR(x) & 255, // FIND UNIT CUBE THAT
            Y = FASTFLOOR(y) & 255, // CONTAINS POINT.
            Z = FASTFLOOR(z) & 255;
        x -= FASTFLOOR(x); // FIND RELATIVE X,Y,Z
        y -= FASTFLOOR(y); // OF POINT IN CUBE.
        z -= FASTFLOOR(z);
#undef FASTFLOOR
        float u = fade(x), // COMPUTE FADE CURVES
            v = fade(y),   // FOR EACH OF X,Y,Z.
            w = fade(z);
        int A = permutation[X] + Y, AA = permutation[A] + Z, AB = permutation[A + 1] + Z,     // HASH COORDINATES OF
            B = permutation[X + 1] + Y, BA = permutation[B] + Z, BB = permutation[B + 1] + Z; // THE 8 CUBE CORNERS,

        return lerp(w, lerp(v, lerp(u, grad(permutation[AA], x, y, z),        // AND ADD
                                    grad(permutation[BA], x - 1, y, z)),      // BLENDED
                            lerp(u, grad(permutation[AB], x, y - 1, z),       // RESULTS
                                 grad(permutation[BB], x - 1, y - 1, z))),    // FROM  8
                    lerp(v, lerp(u, grad(permutation[AA + 1], x, y, z - 1),   // CORNERS
                                 grad(permutation[BA + 1], x - 1, y, z - 1)), // OF CUBE
                         lerp(u, grad(permutation[AB + 1], x, y - 1, z - 1),
                              grad(permutation[BB + 1], x - 1, y - 1, z - 1))));
*/
    }
    template <typename T>
    typename std::enable_if<std::is_integral<T>::value, void>::type GetNoiseSet(const vec3i &pos, const vec3i &size, float frequency, uint8_t octaves, T *out)
    {
        float amplitude = (1 << (sizeof(T) << 3)) - 1;
        float inv_frequency = 1.0f / frequency;
        float half_amplitude = amplitude * 0.5f;

        if(size.y == 1)
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
    typename std::enable_if<std::is_floating_point<T>::value, void>::type GetNoiseSet(const vec3f &pos, const vec3i &size, float frequency, uint8_t octaves, T *out)
    {
        T amplitude = 1.0f;
        T inv_frequency = 1.0f / frequency;
        T half_amplitude = amplitude * 0.5f;
        if(size.y == 1)
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

    uint64_t PositionToSeed(const vec3i &pos)
    {
        return uint64_t(pos.x) * 341873128712 + uint64_t(pos.y) * 132897987541 + uint64_t(pos.z) * 33489798711;
    }

    void FillInitialNoiseSet(vec3i pos, uint32_t width, uint32_t height, uint32_t stride, uint8_t *cell_arr, uint32_t seed)
    {
        JavaLCGInit(PositionToSeed(pos));
        for (uint32_t y = 0, i = 0; y < height; y++, i += stride)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                uint32_t val = JavaLCG() % 100;
                cell_arr[x + i] = val < 47 ? 1 : 0;
            }
        }
    }

    vec3i GetAxisOffset(int axis, vec3i out_size, bool which)
    {
        switch (axis)
        {
        case 0:
            return vec3i(which * out_size.x, !which * out_size.y, 0);
        case 1:
            return vec3i(0, which * out_size.y, !which * out_size.z);
        case 2:
            return vec3i(!which * out_size.x, 0, which * out_size.z);
        }
        return vec3i(0, 0, 0);
    }

    void GetCellularNoiseSet(vec3i pos, vec3i out_size, uint8_t *out, uint32_t seed)
    {
        constexpr int threshold = 4;
        constexpr int iterations = 3;

        vec3i size = out_size * 3;
        uint8_t cell_arr_a[uint32_t(size.x * size.y)];
        uint8_t cell_arr_b[uint32_t(size.y * size.z)];
        uint8_t cell_arr_c[uint32_t(size.z * size.x)];

        uint8_t *cell_arr[3] = {cell_arr_a, cell_arr_b, cell_arr_c};
        uint32_t widths[3] = {uint32_t(size.x), uint32_t(size.y), uint32_t(size.z)};
        uint32_t heights[3] = {uint32_t(size.y), uint32_t(size.z), uint32_t(size.x)};
        uint32_t out_widths[3] = {uint32_t(out_size.x), uint32_t(out_size.y), uint32_t(out_size.z)};
        uint32_t out_heights[3] = {uint32_t(out_size.y), uint32_t(out_size.z), uint32_t(out_size.x)};

        for (uint32_t i = 0; i < 3; i++)
        {
            uint32_t width = widths[i];
            uint32_t height = heights[i];
            uint32_t out_width = out_widths[i];
            uint32_t out_height = out_heights[i];

            uint8_t *sim_arr = cell_arr[i];
            for (int x = -1; x <= 1; x++)
                for (int y = -1; y <= 1; y++)
                {
                    vec3i offset(x * out_size.x, y * out_size.y, 0);
                    if (i == 1)
                        offset = vec3i(0, x * out_size.y, y * out_size.z);
                    else if (i == 2)
                        offset = vec3i(y * out_size.x, 0, x * out_size.z);

                    int sim_off_x = (x + 1) * out_width;
                    int sim_off_y = (y + 1) * out_height;

                    int off = sim_off_x + sim_off_y * width;
                    FillInitialNoiseSet(pos + offset, out_width, out_height, width, &sim_arr[off], seed);
                }
            for (int iter = 0; iter < iterations; iter++)
            {
                for (uint32_t y = 0, j = 0; y < height; y++)
                    for (uint32_t x = 0; x < width; x++, j++)
                    {
                        if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
                            continue;

                        uint8_t neighbors = 0;
                        for (int k = -1; k <= 1; k++)
                        {
                            for (int l = -1; l <= 1; l++)
                            {
                                if ((sim_arr[j + k * width + l] & 1))
                                    neighbors++;
                            }
                        }
                        sim_arr[j] = (sim_arr[j] & 1) | (neighbors << 4);
                    }

                for (uint32_t y = 0, j = 0; y < height; y++)
                    for (uint32_t x = 0; x < width; x++, j++)
                    {
                        if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
                            continue;
                        uint8_t &cell = sim_arr[j];
                        uint8_t neighbors = cell >> 4;
                        cell = (neighbors > threshold);
                    }
            }
        }
        for (int y = out_size.y; y < out_size.y << 1; y++)
            for (int z = out_size.z; z < out_size.z << 1; z++)
                for (int x = out_size.x; x < out_size.x << 1; x++, out++)
                {
                    // AND the three arrays together
                    *out = cell_arr[0][y * size.x + x] & cell_arr[1][z * size.y + y] & cell_arr[2][x * size.z + z];
                }
    }
};
#endif
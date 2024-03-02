#ifndef IMPROVED_NOISE_HPP
#define IMPROVED_NOISE_HPP

#include <cstdint>
#include <cmath>

#define FASTFLOOR(x) (((x) > 0) ? ((int)x) : ((int)x - 1))
extern uint8_t impnoise_permutation[];
class ImprovedNoise
{
public:
    void Init(uint32_t seed);
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

    inline float GetNoise(float x, float y, float z)
    {
        int X = FASTFLOOR(x) & 255, // FIND UNIT CUBE THAT
            Y = FASTFLOOR(y) & 255, // CONTAINS POINT.
            Z = FASTFLOOR(z) & 255;
        x -= FASTFLOOR(x); // FIND RELATIVE X,Y,Z
        y -= FASTFLOOR(y); // OF POINT IN CUBE.
        z -= FASTFLOOR(z);
        float u = fade(x), // COMPUTE FADE CURVES
            v = fade(y),   // FOR EACH OF X,Y,Z.
            w = fade(z);
        int A = impnoise_permutation[X] + Y, AA = impnoise_permutation[A] + Z, AB = impnoise_permutation[A + 1] + Z,     // HASH COORDINATES OF
            B = impnoise_permutation[X + 1] + Y, BA = impnoise_permutation[B] + Z, BB = impnoise_permutation[B + 1] + Z; // THE 8 CUBE CORNERS,

        return lerp(w, lerp(v, lerp(u, grad(impnoise_permutation[AA], x, y, z),        // AND ADD
                                    grad(impnoise_permutation[BA], x - 1, y, z)),      // BLENDED
                            lerp(u, grad(impnoise_permutation[AB], x, y - 1, z),       // RESULTS
                                 grad(impnoise_permutation[BB], x - 1, y - 1, z))),    // FROM  8
                    lerp(v, lerp(u, grad(impnoise_permutation[AA + 1], x, y, z - 1),   // CORNERS
                                 grad(impnoise_permutation[BA + 1], x - 1, y, z - 1)), // OF CUBE
                         lerp(u, grad(impnoise_permutation[AB + 1], x, y - 1, z - 1),
                              grad(impnoise_permutation[BB + 1], x - 1, y - 1, z - 1))));
    }

    inline void GetNoiseSet(float pos_x, float pos_y, float pos_z, uint32_t size_x, uint32_t size_y, uint32_t size_z, float frequency, uint8_t octaves, uint8_t *out)
    {
        float amplitude = 255.0f;
        float inv_frequency = 1.0f / frequency;
        float half_amplitude = amplitude * 0.5f;

        float _y = pos_y * inv_frequency;
        for (uint32_t j = 0; j < size_y; j++, _y += inv_frequency)
        {
            float _z = pos_z * inv_frequency;
            for (uint32_t i = 0; i < size_z; i++, _z += inv_frequency)
            {
                float _x = pos_x * inv_frequency;
                for (uint32_t k = 0; k < size_x; k++, _x += inv_frequency)
                {
                    float val = 0;
                    for (uint8_t i = 0, octave_multiplier = 1; i < octaves; i++)
                    {
                        val += GetNoise(_x * octave_multiplier, _y * octave_multiplier, _z * octave_multiplier) / octave_multiplier;
                        octave_multiplier <<= 1;
                    }
                    *(out++) = uint8_t(half_amplitude + (half_amplitude * val));
                }
            }
        }
    }
};
#endif
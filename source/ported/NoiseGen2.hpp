#pragma once

#include <cstdint>
#include <cmath>
#include <unistd.h>
#include "Random.hpp"

static const int noise_vectors[12][3] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};

class NoiseGenerator2
{
private:
    static constexpr float freq_a = 0.5 * (M_SQRT3 - 1.0);

    static constexpr float freq_b = (3.0 - M_SQRT3) / 6.0;

    static int wrap(float v)
    {
        return v > 0.0 ? (int)v : (int)v - 1;
    }

    static float interpolate(const int *cube, float a, float b)
    {
        return (float)cube[0] * a + (float)cube[1] * b;
    }

    int permutations[512];

public:
    float x_coord;

    float z_coord;

    float y_coord;

    NoiseGenerator2(javaport::Random &rng)
    {
        this->x_coord = rng.nextDouble() * 256.0;
        this->z_coord = rng.nextDouble() * 256.0;
        this->y_coord = rng.nextDouble() * 256.0;

        for (int i = 0; i < 256; i++)
            permutations[i] = i;

        for (int i = 0; i < 256; ++i)
        {
            int other = rng.nextInt(256 - i) + i;
            int orig = this->permutations[i];
            this->permutations[i] = this->permutations[other];
            this->permutations[other] = orig;
            this->permutations[i + 256] = this->permutations[i];
        }
    }

    NoiseGenerator2()
    {
        javaport::Random rng;
        this->x_coord = rng.nextDouble() * 256.0;
        this->z_coord = rng.nextDouble() * 256.0;
        this->y_coord = rng.nextDouble() * 256.0;

        for (int i = 0; i < 256; i++)
            permutations[i] = i;

        for (int i = 0; i < 256; ++i)
        {
            int other = rng.nextInt(256 - i) + i;
            int orig = this->permutations[i];
            this->permutations[i] = this->permutations[other];
            this->permutations[other] = orig;
            this->permutations[i + 256] = this->permutations[i];
        }
    }

    void sample_batch(float *output, float off_x, float off_z, int start_x, int start_z, float x_freq, float z_freq, float amplitude)
    {
        int out_index = 0;

        for (int x = 0; x < start_x; ++x)
        {
            float x_position = (off_x + (float)x) * x_freq + this->x_coord;

            for (int z = 0; z < start_z; ++z)
            {
                float z_position = (off_z + (float)z) * z_freq + this->z_coord;
                float combined_a = (x_position + z_position) * freq_a;
                int x_cube = wrap(x_position + combined_a);
                int z_cube = wrap(z_position + combined_a);
                float combined_b = (float)(x_cube + z_cube) * freq_b;
                float within_cube_x = (float)x_cube - combined_b;
                float within_cube_z = (float)z_cube - combined_b;
                float off_within_cube_x = x_position - within_cube_x;
                float off_within_cube_z = z_position - within_cube_z;
                uint8_t axis_x;
                uint8_t axis_z;
                if (off_within_cube_x > off_within_cube_z)
                {
                    axis_x = 1;
                    axis_z = 0;
                }
                else
                {
                    axis_x = 0;
                    axis_z = 1;
                }

                float off_freq_x = off_within_cube_x - (float)axis_x + freq_b;
                float off_freq_z = off_within_cube_z - (float)axis_z + freq_b;
                float norm_x = off_within_cube_x - 1.0 + 2.0 * freq_b;
                float norm_z = off_within_cube_z - 1.0 + 2.0 * freq_b;
                int cube_local_x = x_cube & 255;
                int cube_local_z = z_cube & 255;
                int perm_a = this->permutations[cube_local_x + this->permutations[cube_local_z]] % 12;
                int perm_b = this->permutations[cube_local_x + axis_x + this->permutations[cube_local_z + axis_z]] % 12;
                int perm_c = this->permutations[cube_local_x + 1 + this->permutations[cube_local_z + 1]] % 12;
                float half_sqr_dist_a = 0.5 - off_within_cube_x * off_within_cube_x - off_within_cube_z * off_within_cube_z;
                float v0;
                if (half_sqr_dist_a < 0.0)
                {
                    v0 = 0.0;
                }
                else
                {
                    half_sqr_dist_a *= half_sqr_dist_a;
                    v0 = half_sqr_dist_a * half_sqr_dist_a * interpolate(noise_vectors[perm_a], off_within_cube_x, off_within_cube_z);
                }

                float half_sqr_dist_b = 0.5 - off_freq_x * off_freq_x - off_freq_z * off_freq_z;
                float v1;
                if (half_sqr_dist_b < 0.0)
                {
                    v1 = 0.0;
                }
                else
                {
                    half_sqr_dist_b *= half_sqr_dist_b;
                    v1 = half_sqr_dist_b * half_sqr_dist_b * interpolate(noise_vectors[perm_b], off_freq_x, off_freq_z);
                }

                float half_sqr_dist_c = 0.5 - norm_x * norm_x - norm_z * norm_z;
                float v2;
                if (half_sqr_dist_c < 0.0)
                {
                    v2 = 0.0;
                }
                else
                {
                    half_sqr_dist_c *= half_sqr_dist_c;
                    v2 = half_sqr_dist_c * half_sqr_dist_c * interpolate(noise_vectors[perm_c], norm_x, norm_z);
                }

                output[out_index++] += 70.0 * (v0 + v1 + v2) * amplitude;
            }
        }
        usleep(0);
    }
};
#pragma once
#include "Random.hpp"
#include <unistd.h>

template <typename T>
class NoiseGeneratorPerlin
{
public:
    T x_coord;
    T y_coord;
    T z_coord;

    NoiseGeneratorPerlin(javaport::Random &rng)
    {
        // Add random offset for this generator
        x_coord = static_cast<T>(rng.nextDouble() * 256.0);
        y_coord = static_cast<T>(rng.nextDouble() * 256.0);
        z_coord = static_cast<T>(rng.nextDouble() * 256.0);

        for (int i = 0; i < 256; i++)
            permutations[i] = i;

        // Init permutations
        for (int i = 0; i < 256; i++)
        {
            int ind = rng.nextInt(256 - i) + i;
            int tmp = permutations[i];
            permutations[i] = permutations[ind];
            permutations[ind] = tmp;
            permutations[i + 256] = permutations[i];
        }
    }

    int permutations[512];

    inline T lerp(T t, T a, T b) { return a + t * (b - a); }
    inline T grad2(int hash, T x, T y)
    {
        int h = hash & 15;
        T u = (T)(1 - ((h & 8) >> 3)) * x;
        T v = h < 4 ? 0.0f : (h != 12 && h != 14 ? y : x);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
    inline T grad3(int hash, T x, T y, T z)
    {
        int h = hash & 15;
        T u = h < 8 ? x : y;
        T v = h < 4 ? y : (h != 12 && h != 14 ? z : x);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

    T sample3d(T x, T y, T z)
    {
        T x_off = x + x_coord;
        T y_off = y + y_coord;
        T z_off = z + z_coord;

        int x_cube = static_cast<int>(x_off);
        int y_cube = static_cast<int>(y_off);
        int z_cube = static_cast<int>(z_off);

        if (x_off < static_cast<T>(x_cube))
            --x_cube;
        if (y_off < static_cast<T>(y_cube))
            --y_cube;
        if (z_off < static_cast<T>(z_cube))
            --z_cube;

        int x_rel = x_cube & 255;
        int y_rel = y_cube & 255;
        int z_rel = z_cube & 255;

        x_off -= static_cast<T>(x_cube);
        y_off -= static_cast<T>(y_cube);
        z_off -= static_cast<T>(z_cube);

        constexpr T coeff_a = static_cast<T>(6.0);
        constexpr T coeff_b = static_cast<T>(15.0);
        constexpr T coeff_c = static_cast<T>(10.0);
        constexpr T one = static_cast<T>(1.0);

        T a = x_off * x_off * x_off * (x_off * (x_off * coeff_a - coeff_b) + coeff_c);
        T b = y_off * y_off * y_off * (y_off * (y_off * coeff_a - coeff_b) + coeff_c);
        T c = z_off * z_off * z_off * (z_off * (z_off * coeff_a - coeff_b) + coeff_c);

        int s0 = permutations[x_rel] + y_rel;
        int s1 = permutations[s0] + z_rel;
        int s2 = permutations[s0 + 1] + z_rel;
        int s3 = permutations[x_rel + 1] + y_rel;
        int s4 = permutations[s3] + z_rel;
        int s5 = permutations[s3 + 1] + z_rel;
        return lerp(c, lerp(b, lerp(a, grad3(permutations[s1], x_off, y_off, z_off), grad3(permutations[s4], x_off - one, y_off, z_off)), lerp(a, grad3(permutations[s2], x_off, y_off - one, z_off), grad3(permutations[s5], x_off - one, y_off - one, z_off))), lerp(b, lerp(a, grad3(permutations[s1 + 1], x_off, y_off, z_off - one), grad3(permutations[s4 + 1], x_off - one, y_off, z_off - one)), lerp(a, grad3(permutations[s2 + 1], x_off, y_off - one, z_off - one), grad3(permutations[s5 + 1], x_off - one, y_off - one, z_off - one))));
    }

    T sample2d(T x, T y)
    {
        return sample3d(x, y, 0);
    }

    void sample_batch(T *dst, T start_x, T start_y, T start_z, int x_size, int y_size, int z_size, T x_scale, T y_scale, T z_scale, T amplitude)
    {
        // AI used for documentation here.

        constexpr T coeff_a = static_cast<T>(6.0);
        constexpr T coeff_b = static_cast<T>(15.0);
        constexpr T coeff_c = static_cast<T>(10.0);
        constexpr T one = static_cast<T>(1.0);
        constexpr T zero = static_cast<T>(0.0);

        /*
         * ---------------------------------------------------------------
         * SPECIAL CASE: 2D NOISE
         * ---------------------------------------------------------------
         *
         * With only one Y layer there is no useful interpolation along Y.
         * The original implementation therefore treats the lattice as
         * two-dimensional and calculates noise using only X and Z.
         */

        if (y_size == 1)
        {

            const T amplitude_scale = one / amplitude;
            int output_index = 0;

            for (int x = 0; x < x_size; ++x)
            {

                /*
                 * Convert the sample coordinate into:
                 *
                 *     lattice x = floor(world x)
                 *     local x   = world x - lattice x
                 *
                 * The explicit floor handling is necessary because Java's
                 * integer cast truncates toward zero rather than toward
                 * negative infinity.
                 */
                T x_coordinate = (start_x + x) * x_scale + x_coord;

                int lattice_x = (int)x_coordinate;

                if (x_coordinate < lattice_x)
                    --lattice_x;

                int lattice_x_wrapped = lattice_x & 255;

                T local_x = x_coordinate - lattice_x;

                /*
                 * Quintic fade curve:
                 *
                 *     fade(local x)
                 *
                 * This controls interpolation between the lattice points.
                 */
                T fade_x = local_x * local_x * local_x * (local_x * (local_x * coeff_a - coeff_b) + coeff_c);

                for (int z = 0; z < z_size; ++z)
                {

                    T z_coordinate = (start_z + z) * z_scale + z_coord;

                    int lattice_z = (int)z_coordinate;

                    if (z_coordinate < lattice_z)
                        --lattice_z;

                    int lattice_z_wrapped = lattice_z & 255;

                    T local_z = z_coordinate - lattice_z;

                    T fade_z = local_z * local_z * local_z * (local_z * (local_z * coeff_a - coeff_b) + coeff_c);

                    /*
                     * Hash the four corners of the current X/Z lattice cell.
                     *
                     * The permutation table effectively converts:
                     *
                     * (lattice x, lattice z)
                     *
                     * into four pseudo-random gradient indices.
                     */
                    int hash_x0 = permutations[lattice_x_wrapped];

                    int hash_00 = permutations[hash_x0] + lattice_z_wrapped;

                    int hash_x1 = permutations[lattice_x_wrapped + 1];

                    int hash_10 = permutations[hash_x1] + lattice_z_wrapped;

                    /*
                     * Evaluate the gradient at the four corners.
                     *
                     * The first corner is at (0, 0).
                     * The other corners are represented using coordinates
                     * offset by -1 because the gradient receives the vector
                     * from the lattice point to the sample position.
                     */
                    T noise00 = lerp(
                        fade_x,
                        grad2(
                            permutations[hash_00],
                            local_x,
                            local_z),
                        grad3(
                            permutations[hash_10],
                            local_x - one,
                            zero,
                            local_z));

                    T noise01 = lerp(
                        fade_x,
                        grad3(
                            permutations[hash_00 + 1],
                            local_x,
                            zero,
                            local_z - one),
                        grad3(
                            permutations[hash_10 + 1],
                            local_x - one,
                            zero,
                            local_z - one));

                    /*
                     * Interpolate between the two X-interpolated values
                     * along Z.
                     */
                    T noise = lerp(fade_z, noise00, noise01);

                    dst[output_index++] += noise * amplitude_scale;
                }
            }
        }
        else
        {

            /*
             * ---------------------------------------------------------------
             * GENERAL CASE: 3D NOISE
             * ---------------------------------------------------------------
             */

            const T amplitude_scale = one / amplitude;

            int output_index = 0;

            /*
             * latticeY is deliberately retained between iterations.
             *
             * Y is the innermost loop, so consecutive samples normally
             * remain inside the same Y lattice cell. When that happens,
             * all eight gradient hashes for the surrounding 3D cell can
             * be reused.
             */
            int previous_lattice_y = -1;

            /*
             * These hold the four values produced after interpolating
             * along X for the bottom/front/back/etc. faces of the cube.
             *
             * More conceptually, they are the four X-interpolated edges
             * of the current 3D lattice cube:
             *
             *       y=1
             *        +---------+
             *       /|        /|
             *      +---------+ |
             *      | |       | |
             *      | +-------|-+
             *      |/        |/
             *      +---------+
             *       y=0
             */
            T x_interp000 = zero;
            T x_interp001 = zero;
            T x_interp010 = zero;
            T x_interp011 = zero;

            for (int x = 0; x < x_size; ++x)
            {

                T x_coordinate = (start_x + x) * x_scale + x_coord;

                int lattice_x = (int)x_coordinate;

                if (x_coordinate < lattice_x)
                    --lattice_x;

                int lattice_x_wrapped = lattice_x & 255;

                T local_x = x_coordinate - lattice_x;

                T fade_x = local_x * local_x * local_x * (local_x * (local_x * coeff_a - coeff_b) + coeff_c);

                for (int z = 0; z < z_size; ++z)
                {

                    T z_coordinate = (start_z + z) * z_scale + z_coord;

                    int lattice_z = (int)z_coordinate;

                    if (z_coordinate < lattice_z)
                        --lattice_z;

                    int lattice_z_wrapped = lattice_z & 255;

                    T local_z = z_coordinate - lattice_z;

                    T fade_z = local_z * local_z * local_z * (local_z * (local_z * coeff_a - coeff_b) + coeff_c);

                    for (int y = 0; y < y_size; ++y)
                    {
                        T y_coordinate = (start_y + y) * y_scale + y_coord;

                        int lattice_y = (int)y_coordinate;

                        if (y_coordinate < lattice_y)
                            --lattice_y;

                        int lattice_y_wrapped = lattice_y & 255;

                        T local_y = y_coordinate - lattice_y;

                        T fade_y = local_y * local_y * local_y * (local_y * (local_y * coeff_a - coeff_b) + coeff_c);

                        /*
                         * If we entered a new Y lattice cell, calculate
                         * the eight gradient hashes surrounding the sample.
                         *
                         * A 3D Perlin sample lives inside a cube with
                         * eight corners:
                         *
                         *        011 -------- 111
                         *       / |           / |
                         *      /  |          /  |
                         *   001 -------- 101  |
                         *    |   |         |   |
                         *    |  010 -------|-- 110
                         *    | /           | /
                         *    |/            |/
                         *   000 -------- 100
                         *
                         * The permutation table is used to turn each
                         * lattice coordinate into a deterministic gradient
                         * index.
                         */
                        if (y == 0 || lattice_y_wrapped != previous_lattice_y)
                        {

                            previous_lattice_y = lattice_y_wrapped;

                            /*
                             * First permutation lookup: X + Y.
                             */
                            int hashx0y = permutations[lattice_x_wrapped] + lattice_y_wrapped;

                            /*
                             * Second permutation lookup: X + 1 + Y.
                             */
                            int hashx1y = permutations[lattice_x_wrapped + 1] + lattice_y_wrapped;

                            /*
                             * Combine the Y hashes with Z to obtain the
                             * four corners on each X side.
                             */
                            int hash000 = permutations[hashx0y] + lattice_z_wrapped;

                            int hash001 = permutations[hashx0y + 1] + lattice_z_wrapped;

                            int hash100 = permutations[hashx1y] + lattice_z_wrapped;

                            int hash101 = permutations[hashx1y + 1] + lattice_z_wrapped;

                            /*
                             * Interpolate the four pairs of X-facing
                             * corners along X.
                             *
                             * The names here use:
                             *
                             *     first digit = X
                             *     second digit = Y
                             *     third digit = Z
                             *
                             * although the actual spatial ordering is
                             * less important than the fact that these
                             * represent the four cube edges after X
                             * interpolation.
                             */
                            x_interp000 = lerp(
                                fade_x,
                                grad3(
                                    permutations[hash000],
                                    local_x,
                                    local_y,
                                    local_z),
                                grad3(
                                    permutations[hash100],
                                    local_x - one,
                                    local_y,
                                    local_z));

                            x_interp001 = lerp(
                                fade_x,
                                grad3(
                                    permutations[hash001],
                                    local_x,
                                    local_y - one,
                                    local_z),
                                grad3(
                                    permutations[hash101],
                                    local_x - one,
                                    local_y - one,
                                    local_z));

                            x_interp010 = lerp(
                                fade_x,
                                grad3(
                                    permutations[hash000 + 1],
                                    local_x,
                                    local_y,
                                    local_z - one),
                                grad3(
                                    permutations[hash100 + 1],
                                    local_x - one,
                                    local_y,
                                    local_z - one));

                            x_interp011 = lerp(
                                fade_x,
                                grad3(
                                    permutations[hash001 + 1],
                                    local_x,
                                    local_y - one,
                                    local_z - one),
                                grad3(
                                    permutations[hash101 + 1],
                                    local_x - one,
                                    local_y - one,
                                    local_z - one));
                        }

                        /*
                         * Interpolate the four X-interpolated values
                         * along Y.
                         */
                        T y_interp0 = lerp(fade_y, x_interp000, x_interp001);

                        T y_interp1 = lerp(fade_y, x_interp010, x_interp011);

                        /*
                         * Finally interpolate along Z.
                         *
                         * This completes the trilinear interpolation
                         * across the eight corners of the lattice cube.
                         */
                        T noise = lerp(fade_z, y_interp0, y_interp1);

                        dst[output_index++] += noise * amplitude_scale;
                    }
                }
            }
        }

        // After each generation, it's important to yield a bit.
        // This is done to prevent multiple runs of the noise
        // generator from blocking other threads for too long,
        // which would cause hiccups.
        usleep(0);
    }
};
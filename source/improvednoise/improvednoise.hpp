#ifndef IMPROVED_NOISE_HPP
#define IMPROVED_NOISE_HPP

#include "../vec3f.hpp"
#include "../vec3i.hpp"
#include "../threadhandler.hpp"

class ImprovedNoise
{
private:
    void init();

public:
    ImprovedNoise() { init(); };
    float noise(float x, float y, float z);
    void noise_set(vec3f &pos, vec3i &size, float frequency, float amplitude, uint8_t* out);

    float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    float lerp(float t, float a, float b) { return a + t * (b - a); }
    float grad(int hash, float x, float y, float z)
    {
        int h = hash & 15;       // CONVERT LO 4 BITS OF HASH CODE
        float u = h < 8 ? x : y, // INTO 12 GRADIENT DIRECTIONS.
            v = h < 4 ? y : h == 12 || h == 14 ? x
                                               : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
};
#endif
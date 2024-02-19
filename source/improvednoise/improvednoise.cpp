#include "improvednoise.hpp"
uint8_t impnoise_permutation[512];
void ImprovedNoise::Init(uint32_t seed)
{
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    uint32_t x = seed ? seed : 1;
    for (int i = 0; i < 256; i++)
        impnoise_permutation[i] = i;
    for (int i = 0; i < 256; i++)
    {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        std::swap(impnoise_permutation[x & 0xFF], impnoise_permutation[(~x) & 0xFF]);
    }
    for (int i = 0; i < 256; i++)
        impnoise_permutation[256 + i] = impnoise_permutation[i];
}
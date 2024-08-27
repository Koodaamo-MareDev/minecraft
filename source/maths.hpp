#ifndef MATHS_HPP
#define MATHS_HPP

inline static float Q_rsqrt(float number)
{
    union
    {
        float f;
        unsigned int i;
    } conv = {.f = number};
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f *= 1.5F - (number * 0.5F * conv.f * conv.f);
    return conv.f;
}

inline static double Q_rsqrt_d(double number)
{
    union
    {
        double f;
        unsigned long long i;
    } conv = {.f = number};
    conv.i = 0x5fe6eb50c7b537a9 - (conv.i >> 1);
    conv.f *= 1.5 - (number * 0.5 * conv.f * conv.f);
    return conv.f;
}

#endif
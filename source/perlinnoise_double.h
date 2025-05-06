#ifndef PERLINNOISE_DOUBLE_H
#define PERLINNOISE_DOUBLE_H

#ifdef __cplusplus
extern "C"
{
#endif

double noise1(double arg);
double noise2(double vec[2]);
double noise3(double vec[3]);

void init_noise(void);

#ifdef __cplusplus
}
#endif

#endif
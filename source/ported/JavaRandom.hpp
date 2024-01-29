#ifndef _JAVA_RANDOM_HPP_
#define _JAVA_RANDOM_HPP_

#include <cstdint>
void JavaLCGInit(int64_t seed);
int64_t JavaLCG();
float JavaLCGFloat();
double JavaLCGDouble();
#endif
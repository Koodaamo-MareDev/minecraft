#ifndef _JAVA_RANDOM_HPP_
#define _JAVA_RANDOM_HPP_

#include <cstdint>
#include <cstdio>
#include <string>
void JavaLCGInit(int64_t seed);
void JavaLCGSetState(int64_t seed);
int64_t JavaLCGGetState();
int64_t JavaLCG();
float JavaLCGFloat();
double JavaLCGDouble();
int32_t JavaLCGNext(int32_t bits);
int32_t JavaLCGIntN(int32_t n);
int JavaHashCode(const std::string &value);
#endif
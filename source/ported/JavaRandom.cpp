#include "JavaRandom.hpp"
const float JavaFloatUnit = 1.0f / (0x1000000);
const double JavaDoubleUnit = 1.0 / (0x20000000000000LL);
const int64_t MaxJavaLCG = 0xFFFFFFFFFFFFLL;
int64_t SeedJavaLCG = 0;
const int64_t AJavaLCG = 25214903917;
const int64_t CJavaLCG = 11;
void JavaLCGInit(int64_t seed)
{
    SeedJavaLCG = (seed ^ AJavaLCG) & MaxJavaLCG;
}
int64_t JavaLCG()
{
    SeedJavaLCG = (AJavaLCG * SeedJavaLCG + CJavaLCG) & MaxJavaLCG;
    return SeedJavaLCG;
}
float JavaLCGFloat()
{
    return (JavaLCG() >> 24) * JavaFloatUnit;
}
double JavaLCGDouble()
{
    return (((long)(JavaLCG() >> 26) << 27) + (JavaLCG() >> 21)) * JavaDoubleUnit;
}
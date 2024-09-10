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
void JavaLCGSetState(int64_t seed)
{
    SeedJavaLCG = seed;
}
int64_t JavaLCGGetState()
{
    return SeedJavaLCG;
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
    return (((int64_t)(JavaLCG() >> 26) << 27) + (JavaLCG() >> 21)) * JavaDoubleUnit;
}

int32_t JavaLCGNext(int32_t bits)
{
    return (int32_t)(uint64_t(JavaLCG()) >> (48 - bits));
}

int32_t JavaLCGIntN(int32_t n)
{
    if (n <= 0)
        return 0;

    if ((n & -n) == n) // i.e., n is a power of 2
        return (int32_t)((n * (int64_t)JavaLCGNext(31)) >> 31);

    int32_t bits, val;
    do
    {
        bits = JavaLCGNext(31);
        val = bits % n;
    } while (bits - val + (n - 1) < 0);
    return val;
}

int JavaHashCode(const std::string &value)
{
    int h = 0;
    int len = value.size();
    if (h == 0 && len > 0)
    {
        for (int i = 0; i < len; i++)
        {
            h = 31 * h + value[i];
        }
    }
    return h;
}
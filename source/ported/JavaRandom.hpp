#ifndef _JAVA_RANDOM_HPP_
#define _JAVA_RANDOM_HPP_

#include <cstdint>
#include <cstdio>
#include <string>
namespace javaport
{
    class Random
    {
        constexpr static float float_unit = 1.0f / (0x1000000);
        constexpr static double double_unit = 1.0 / (0x20000000000000LL);
        constexpr static int64_t max_value = 0xFFFFFFFFFFFFLL;
        constexpr static int64_t a_coeff = 25214903917;
        constexpr static int64_t c_coeff = 11;

        int64_t state = 0;

    public:
        Random(int64_t seed);
        Random();
        void setSeed(int64_t seed);
        void setState(int64_t seed);
        int64_t getState();
        int64_t nextInt();
        float nextFloat();
        double nextDouble();
        int32_t next(int32_t bits);
        int32_t nextInt(int32_t n);
        int64_t nextLong();
    };
    int StringHashCode(const std::string &value);
}
#endif
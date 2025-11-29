#include "Random.hpp"
#include "SystemTime.hpp"
namespace javaport
{
    Random::Random(int64_t seed)
    {
        setSeed(seed);
    }

    Random::Random()
    {
        setSeed(System::nanoTime());
    }

    void Random::setSeed(int64_t seed)
    {
        state = (seed ^ a_coeff) & max_value;
    }
    void Random::setState(int64_t seed)
    {
        state = seed;
    }
    int64_t Random::getState()
    {
        return state;
    }
    int64_t Random::nextInt()
    {
        state = (a_coeff * state + c_coeff) & max_value;
        return state;
    }
    float Random::nextFloat()
    {
        return (next(24)) * float_unit;
    }
    double Random::nextDouble()
    {
        return (((int64_t)next(26) << 27) + next(27)) * double_unit;
    }
    int32_t Random::next(int32_t bits)
    {
        return (int32_t)(uint64_t(nextInt()) >> (48 - bits));
    }
    int32_t Random::nextInt(int32_t n)
    {
        if (n <= 0)
            return 0;

        if ((n & -n) == n) // i.e., n is a power of 2
            return (int32_t)((n * (int64_t)next(31)) >> 31);

        int32_t bits, val;
        do
        {
            bits = next(31);
            val = bits % n;
        } while (bits - val + (n - 1) < 0);
        return val;
    }

    int64_t Random::nextLong()
    {
        return ((int64_t)next(32) << 32) + next(32);
    }

    int StringHashCode(const std::string &value)
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
}
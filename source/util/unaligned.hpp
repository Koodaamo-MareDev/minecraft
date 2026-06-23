#include <cstring>
#include <type_traits>
#include <cstdint>

template <typename T>
class __attribute__((packed)) unaligned
{
    static_assert(std::is_trivially_copyable_v<T>);

public:
    unaligned() = default;

    unaligned(const T &value)
    {
        *this = value;
    }

    operator T() const
    {
        T value;
        std::memcpy(&value, storage, sizeof(T));
        return value;
    }

    unaligned &operator=(const T &value)
    {
        std::memcpy(storage, &value, sizeof(T));
        return *this;
    }

private:
    alignas(1) std::uint8_t storage[sizeof(T)];
};
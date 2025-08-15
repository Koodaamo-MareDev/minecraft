#ifndef MCREGION_HPP
#define MCREGION_HPP

#include <cstdint>
#include <deque>
#include <fstream>

namespace mcr
{
    constexpr int32_t MAX_OPEN_FILES = 8;
    class Region
    {
    public:
        int32_t x = 0;
        int32_t z = 0;
        uint32_t locations[1024] = {0};
        uint32_t last_modified[1024] = {0};
        uint32_t allocate(uint32_t size, uint16_t index);

        std::fstream &open();
        void close();
        bool is_open();

    private:
        std::fstream file;
    };

    Region *get_region(int32_t x, int32_t z);

    std::deque<Region *> &get_regions();

    void close_redundant_region(mcr::Region *exclude);
    
    void cleanup();
} // namespace mcr

#endif